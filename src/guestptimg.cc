#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sstream>
#include <sys/syscall.h>
#include "Sugar.h"

#include "elfimg.h"

#include "ptimgchk.h"
#include "guestptimg.h"
#include "guestcpustate.h"

#ifdef __amd64__
#include "amd64cpustate.h"
#endif

using namespace llvm;

static bool dump_maps;

GuestPTImg::GuestPTImg(
	int argc, char *const argv[], char *const envp[])
: Guest(argv[0])
{
	ElfImg		*img;
	
	dump_maps = (getenv("VEXLLVM_DUMP_MAPS")) ? true : false;

	img = ElfImg::create(argv[0], false);
	assert (img != NULL && "DOES BINARY EXIST?");

	entry_pt = img->getEntryPoint();

	assert(img->getArch() == getArch());

	cpu_state = GuestCPUState::create(getArch());

	delete img;
}

void GuestPTImg::handleChild(pid_t pid)
{
	ptrace(PTRACE_KILL, pid, NULL, NULL);
	wait(NULL);
}

pid_t GuestPTImg::createSlurpedChild(
	int argc, char *const argv[], char *const envp[])
{
	int			err, status;
	pid_t			pid;
	void			*break_addr;

	pid = fork();
	if (pid == 0) {
		/* child */
		int     err;
		ptrace(PTRACE_TRACEME, 0, NULL, NULL);
		err = execvpe(argv[0], argv, envp);
		assert (err != -1 && "EXECVE FAILED. NO PTIMG!");
	}

	/* failed to create child */
	if (pid < 0) return pid;

	/* wait for child to call execve and send us a trap signal */
	wait(&status);
	assert (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP);

	/* Trapped the process on execve-- binary is loaded, but not linked */
	/* overwrite entry with BP. */
	setBreakpoint(pid, entry_pt);

	/* go until child hits entry point */
	err = ptrace(PTRACE_CONT, pid, NULL, NULL);
	assert (err != -1);
	wait(&status);
	assert (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP);

	break_addr = undoBreakpoint(pid);
	assert (break_addr == entry_pt && "Did not break at entry");

	/* hit the entry point, everything should be linked now-- load it
	 * into our context! */
	/* cleanup bp */
	resetBreakpoint(pid, entry_pt);

	if (dump_maps) dumpSelfMap();

	/* slurp brains after trap code is removed so that we don't 
	 * copy the trap code into the parent process */
	slurpBrains(pid);

	/* and store the mapping */
	setupMem();

	return pid;
}

void* GuestPTImg::undoBreakpoint(pid_t pid)
{
	struct user_regs_struct	regs;
	int			err;

	/* should be halted on our trapcode. need to set rip prior to 
	 * trapcode addr */
	err = ptrace(PTRACE_GETREGS, pid, NULL, &regs);
	assert (err != -1);

	regs.rip--; /* backtrack before int3 opcode */
	err = ptrace(PTRACE_SETREGS, pid, NULL, &regs);

	/* run again w/out reseting BP and you'll end up back here.. */
	return (void*)regs.rip;
}

int PTImgMapEntry::getProt(void) const
{
	int	prot = 0;

	if (perms[0] == 'r') prot |= PROT_READ;
	if (perms[1] == 'w') prot |= PROT_WRITE;
	if (perms[2] == 'x') prot |= PROT_EXEC;

	return prot;
}

#define STACK_EXTEND_BYTES 0x200000

void PTImgMapEntry::mapStack(pid_t pid)
{
	int			prot, flags;

	assert (mmap_fd == -1);

	prot = getProt();
	flags = MAP_GROWSDOWN | MAP_STACK | MAP_PRIVATE | MAP_ANONYMOUS;

	mem_begin = (void*)((uint64_t)mem_begin - STACK_EXTEND_BYTES);

	mmap_base = mmap(
		mem_begin,
		getByteCount(), 
		prot,
		flags | MAP_FIXED,
		-1,
		0);
	if (mmap_base != mem_begin) {
		fprintf(stderr, "COLLISION: GOT=%p, EXPECTED=%p\n",
			mmap_base, mem_begin);
	}

	assert (mmap_base == mem_begin && "Could not map to same address");
	ptraceCopyRange(pid, prot, 
		(void*)((uintptr_t)mem_begin + STACK_EXTEND_BYTES),
		mem_end);
}

void PTImgMapEntry::mapAnon(pid_t pid)
{
	int			prot, flags;

	assert (mmap_fd == -1);

	prot = getProt();
	flags = MAP_PRIVATE | MAP_ANONYMOUS;

	mmap_base = mmap(
		mem_begin,
		getByteCount(), 
		prot,
		flags | MAP_FIXED,
		mmap_fd,
		off);
	if (mmap_base != mem_begin) {
		fprintf(stderr, "COLLISION: GOT=%p, EXPECTED=%p\n",
			mmap_base, mem_begin);
	}

	assert (mmap_base == mem_begin && "Could not map to same address");
	ptraceCopy(pid, prot);
}

void PTImgMapEntry::mapLib(pid_t pid)
{
	int			prot, flags;


	if (strcmp(libname, "[vsyscall]") == 0) 
		return;
	if (strcmp(libname, "[stack]") == 0) {
		is_stack = true;
		mapStack(pid);
		return;
	}

	mmap_fd = open(libname, O_RDONLY);
	if (mmap_fd == -1) {
		struct stat	s;
		int		rc;

		rc = stat(libname, &s);
		assert (rc == -1);
	
		mapAnon(pid);
			
		return;
	}

	flags = MAP_PRIVATE;
	prot = getProt();
//	if (prot & PROT_EXEC) flags = MAP_SHARED;
//	else flags = MAP_PRIVATE;

	assert (mmap_fd != -1);

	mmap_base = mmap(
		mem_begin,
		getByteCount(), 
		prot,
		flags | MAP_FIXED,
		mmap_fd,
		off);
	if (mmap_base != mem_begin) {
		fprintf(stderr, "COLLISION: GOT=%p, EXPECTED=%p\n",
			mmap_base, mem_begin);
	}

	assert (mmap_base == mem_begin && "Could not map to same address");
	if (flags != MAP_SHARED)
		ptraceCopy(pid, prot);
}

void PTImgMapEntry::ptraceCopyRange(pid_t pid, int prot, void* m_beg, void* m_end)
{
	long			*copy_addr;

	copy_addr = (long*)m_beg;
	errno = 0;
	while (copy_addr != m_end) {
		long	peek_data;
		peek_data = ptrace(PTRACE_PEEKDATA, pid, copy_addr, NULL);
		assert (peek_data != -1 || errno == 0);
		if (*copy_addr != peek_data) {
			*copy_addr = peek_data;
		}

		copy_addr++;
	}
}

void PTImgMapEntry::ptraceCopy(pid_t pid, int prot)
{
	if (!(prot & PROT_READ)) return;

	if (!(prot & PROT_WRITE))
		mprotect(mmap_base, getByteCount(), prot | PROT_WRITE);

	ptraceCopyRange(pid, prot, mem_begin, mem_end);

	if (!(prot & PROT_WRITE))
		mprotect(mmap_base, getByteCount(), prot);
}

PTImgMapEntry::PTImgMapEntry(pid_t pid, const char* mapline)
 : mmap_base(NULL), mmap_fd(-1), is_stack(false)
{
	int                     rc;

	libname[0] = '\0';
	rc = sscanf(mapline, "%p-%p %s %x %d:%d %d %s",
		&mem_begin,
		&mem_end,
		perms,
		&off,
		&t[0],
		&t[1],
		&xxx,
		libname);

	assert (rc >= 0);

	if (dump_maps) fprintf(stderr, "MAPPING: %s", mapline);

	/* now map it in */
	if (strlen(libname) > 0)
		mapLib(pid);
	else 
		mapAnon(pid);
}

PTImgMapEntry::~PTImgMapEntry(void)
{
	if (mmap_fd) close(mmap_fd);
	if (mmap_base) munmap(mmap_base, getByteCount());
}

void GuestPTImg::dumpSelfMap(void)
{
	FILE	*f_self;
	char	buf[256];

	f_self = fopen("/proc/self/maps", "r");
	while (!feof(f_self)) {
		if (fgets(buf, 256, f_self) == NULL)
			break;
		fprintf(stderr, "[ME] %s", buf);
	}
	fclose(f_self);
}

void GuestPTImg::slurpMappings(pid_t pid)
{
	FILE	*f;
	char	map_fname[256];

	sprintf(map_fname, "/proc/%d/maps", pid);
	f = fopen(map_fname, "r");
	while (!feof(f)) {
		char		line_buf[256];
		PTImgMapEntry	*mapping;

		if (fgets(line_buf, 256, f) == NULL)
			break;

		mapping = new PTImgMapEntry(pid, line_buf);
		mappings.add(mapping);
	}
	fclose(f);
}

void GuestPTImg::slurpRegisters(pid_t pid)
{
	int				err;
	struct user_regs_struct		regs;
	struct user_fpregs_struct	fpregs;

	err = ptrace(PTRACE_GETREGS, pid, NULL, &regs); 
	assert(err != -1);
	err = ptrace(PTRACE_GETFPREGS, pid, NULL, &fpregs); 
	assert(err != -1);

#ifdef __amd64__
	AMD64CPUState* amd64_cpu_state = (AMD64CPUState*)cpu_state;
	amd64_cpu_state->setRegs(regs, fpregs);
	amd64_cpu_state->setTLS(new PTImgTLS((void*)regs.fs_base));
#endif
}

void GuestPTImg::slurpBrains(pid_t pid)
{
	slurpMappings(pid);
	slurpRegisters(pid);
}

void GuestPTImg::stackTrace(
	std::ostream& os,
	const char* binname, pid_t pid,
	void* range_begin, void* range_end)
{
	char			buffer[1024];
	int			bytes;
	int			pipefd[2];
	int			err;
	std::ostringstream	pid_str, disasm_cmd;

	pid_str << pid;
	if (range_begin && range_end) {
		disasm_cmd << "disass " << range_begin << "," << range_end; 
	} else {
		disasm_cmd << "print \"???\"";
	}
	
	err = pipe(pipefd);
	assert (err != -1 && "Bad pipe for subservient trace");
	
	kill(pid, SIGSTOP);
	ptrace(PTRACE_DETACH, pid, NULL, NULL);
	
	if (!fork()) {
		close(pipefd[0]);    // close reading end in the child
		dup2(pipefd[1], 1);  // send stdout to the pipe
		dup2(pipefd[1], 2);  // send stderr to the pipe
		close(pipefd[1]);    // this descriptor is no longer needed

		execl("/usr/bin/gdb", 
			"/usr/bin/gdb",
			"--batch",
			binname,
			pid_str.str().c_str(),
			"--eval-command",
			"thread apply all bt",
			"--eval-command",
			"print \"disasm of where guest ended (may fail)\"",
			"--eval-command",
			"disass",
			 "--eval-command",
			"print \"disasm of block with error (just finished)\"",
			"--eval-command",
			disasm_cmd.str().c_str(),
			"--eval-command",
			"info registers all",
			"--eval-command",
			"kill",
			NULL
		);
		exit(1);
	}

	  // close the write end of the pipe in the parent
	close(pipefd[1]);

	while ((bytes = read(pipefd[0], buffer, sizeof(buffer))) != 0)
		os.write(buffer, bytes);
}

/* TODO: only change a single character */
void GuestPTImg::setBreakpoint(pid_t pid, void* addr)
{
	uint64_t		old_v, new_v;
	int			err;

	/* already set? */
	if (breakpoints.count(addr)) return;

	old_v = ptrace(PTRACE_PEEKTEXT, pid, addr, NULL);
	new_v = old_v & ~0xff;
	new_v |= 0xcc;

	err = ptrace(PTRACE_POKETEXT, pid, addr, new_v);
	assert (err != -1 && "Failed to set breakpoint");
	breakpoints[addr] = old_v;
}

void GuestPTImg::resetBreakpoint(pid_t pid, void* addr)
{
	uint64_t	old_v;
	int		err;

	assert (breakpoints.count(addr) && "Resetting non-BP!");

	old_v = breakpoints[addr];
	err = ptrace(PTRACE_POKETEXT, pid, addr, old_v);
	assert (err != -1 && "Failed to reset breakpoint");
	breakpoints.erase(addr);
}

void GuestPTImg::setupMem(void)
{
	assert (mem == NULL);
	mem = new GuestMem();

	foreach (it, mappings.begin(), mappings.end()) {
		PTImgMapEntry		*ptm = *it;
		GuestMem::Mapping	s(
			ptm->getBase(),
			ptm->getByteCount(),
			ptm->getProt(),
			ptm->isStack());

		mem->recordMapping(s);
	}	
	//TODO: guess the brk? maybe not necessary for ptimg
}

Arch::Arch GuestPTImg::getArch() const {  return Arch::getHostArch(); }

