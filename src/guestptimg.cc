#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stddef.h>
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
#include <sys/time.h>
#include <set>
#include "Sugar.h"

#include "util.h"
#include "symbols.h"
#include "elfimg.h"
#include "elfdebug.h"
#include "ptimgchk.h"
#include "guestptimg.h"
#include "guestcpustate.h"

#if defined(__amd64__)
#include <asm/ptrace-abi.h>
#include <sys/prctl.h>
#include "cpu/amd64cpustate.h"
#include "cpu/ptimgamd64.h"
#endif

#ifdef __arm__
#include <linux/prctl.h>
#include <sys/prctl.h>
#include "cpu/ptimgarm.h"
#endif

using namespace llvm;

static bool dump_maps;

GuestPTImg::GuestPTImg(
	int argc, char *const argv[], char *const envp[],
	bool use_entry)
: Guest(argv[0])
, pt_arch(NULL)
, symbols(NULL)
, dyn_symbols(NULL)
, arch(Arch::getHostArch())
{
	mem = new GuestMem();
	dump_maps = (getenv("VEXLLVM_DUMP_MAPS")) ? true : false;

	entry_pt = guest_ptr(0);
	if (argv[0] != NULL && use_entry)  {
		ElfImg	*img = ElfImg::create(argv[0], false);
		assert (img != NULL && "DOES BINARY EXIST?");
		assert(img->getArch() == getArch());

		entry_pt = img->getEntryPoint();
		delete img;
	}

	cpu_state = GuestCPUState::create(getArch());
}

GuestPTImg::~GuestPTImg(void)
{
	if (pt_arch != NULL) delete pt_arch;
	if (symbols != NULL) delete symbols;
	if (dyn_symbols != NULL) delete dyn_symbols;
}

void GuestPTImg::handleChild(pid_t pid)
{
	ptrace(PTRACE_KILL, pid, NULL, NULL);
	wait(NULL);
}


#if defined(__amd64__)
#define NEW_ARCH	new PTImgAMD64(this, pid);
#elif defined(__arm__)
#define NEW_ARCH	new PTImgARM(this, pid);
#else
#define NEW_ARCH	0; assert (0 == 1 && "UNKNOWN PTRACE HOST ARCHITECTURE! AIEE");
#endif


pid_t GuestPTImg::createSlurpedAttach(int pid)
{
	int	err, status;

	// assert (entry_pt.o == 0 && "Only support attaching immediately");
	fprintf(stderr, "Attaching to PID=%d\n", pid);

	pt_arch = NEW_ARCH;

	err = ptrace(PTRACE_ATTACH, pid, 0, NULL, NULL);
	assert (err != -1 && "Couldn't attach to process");

	wait(&status);
	fprintf(stderr, "ptrace status=%p\n", (void*)status);
	assert (WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP);
	fprintf(stderr, "Attached to PID=%d\n", pid);

	err = ptrace(PTRACE_SYSCALL, pid, 0, NULL, NULL);
	wait(&status);
	assert (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP);
//	fprintf(stderr, "Got syscall from PID=%d\n", pid);
//
#if defined(__amd64__)

	/* slurp brains after trap code is removed so that we don't
	 * copy the trap code into the parent process */
	slurpBrains(pid);

	/* URK. syscall's RAX isn't stored in RAX! */
	struct user_regs_struct	r;
	err = ptrace(__ptrace_request(PTRACE_GETREGS), pid, NULL, &r);

	uint8_t* x = (uint8_t*)r.rip;
	assert (((x[-1] == 0x05 && x[-2] == 0x0f) /* syscall */ ||
		(x[-2] == 0xcd && x[-1] == 0x80) /* int0x80 */)
		&& "not syscall opcode?");
	getCPUState()->setPC(guest_ptr(getCPUState()->getPC()-2));
	getCPUState()->setSyscallResult(r.orig_rax);

	if (x[-2] == 0xcd) {
		/* XXX:int only used by i386, never amd64? */
		arch = Arch::I386;
	}
#else
	assert (0 == 1 && "CAN'T HANDLE THIS");
#endif
	entry_pt = getCPUState()->getPC();

	/* catch and release */
	// ptrace(PTRACE_CONT, pid, 0, NULL, NULL);
	ptrace(PTRACE_DETACH, pid, 0, NULL, NULL);

	return pid;
}

pid_t GuestPTImg::createSlurpedChild(
	int argc, char *const argv[], char *const envp[])
{
	int			err, status;
	pid_t			pid;
	guest_ptr		break_addr;

	assert (entry_pt.o && "No entry point given to slurp");

	pid = fork();
	if (pid == 0) {
		/* child */
		int     err;

		/* don't keep running if parent dies */
                prctl(PR_SET_PDEATHSIG, SIGKILL);
                prctl(PR_SET_PDEATHSIG, SIGSEGV);
                prctl(PR_SET_PDEATHSIG, SIGILL);
                prctl(PR_SET_PDEATHSIG, SIGBUS);
                prctl(PR_SET_PDEATHSIG, SIGABRT);
		ptrace(PTRACE_TRACEME, 0, NULL, NULL);
		if (getenv("VEXLLVM_PRELOAD")) {
			setenv("LD_PRELOAD", getenv("VEXLLVM_PRELOAD"), 1);
			err = execvp(argv[0], argv);
		} else
			err = execvpe(argv[0], argv, envp);
		assert (err != -1 && "EXECVE FAILED. NO PTIMG!");
	}

	/* failed to create child */
	if (pid < 0) return pid;

	pt_arch = NEW_ARCH;

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

#ifdef __amd64__
	/* XXX there should be a better place for this */
	/* from glibc, sysdeps/x86_64/elf/start.S
	 * argc = rsi
	 * argv = rdx */
	guest_ptr		in_argv;

	argv_ptrs.clear();
	argc = mem->readNative(pt_arch->getStackPtr());
	in_argv = guest_ptr(mem->readNative(pt_arch->getStackPtr(), 1));
	for (int i = 0; i < argc; i++) {
		argv_ptrs.push_back(in_argv);
		in_argv.o += mem->strlen(in_argv) + 1;
	}
#else
	fprintf(stderr,
		"[VEXLLVM] WARNING: can not get argv for current arch\n");
#endif


	return pid;
}

int PTImgMapEntry::getProt(void) const
{
	int	prot = 0;

	if (perms[0] == 'r') prot |= PROT_READ;
	if (perms[1] == 'w') prot |= PROT_WRITE;
	if (perms[2] == 'x') prot |= PROT_EXEC;

	return prot;
}

#ifdef __amd64__
#define STACK_EXTEND_BYTES	0x200000
#else
#define STACK_EXTEND_BYTES	0x1000
#endif

#if defined(__arm__)
#define STACK_MAP_FLAGS	\
	(MAP_GROWSDOWN | MAP_PRIVATE | MAP_ANONYMOUS)
#else
#define STACK_MAP_FLAGS \
	(MAP_GROWSDOWN | MAP_STACK | MAP_PRIVATE | MAP_ANONYMOUS)
#endif


void PTImgMapEntry::mapStack(pid_t pid)
{
	int			prot;

	assert (mmap_fd == -1);

	prot = getProt();

	mem_begin.o -= STACK_EXTEND_BYTES;

	int res = mem->mmap(
		mmap_base,
		mem_begin,
		getByteCount(),
		prot,
		STACK_MAP_FLAGS | MAP_FIXED,
		-1,
		0);

	assert (!res && "Could not map stack region");

	ptraceCopyRange(pid,
		mem_begin + STACK_EXTEND_BYTES,
		mem_end);
}

void PTImgMapEntry::mapAnon(pid_t pid)
{
	int			prot, flags;

	assert (mmap_fd == -1);

	prot = getProt();
	flags = MAP_PRIVATE | MAP_ANONYMOUS;

	int res = mem->mmap(
		mmap_base,
		mem_begin,
		getByteCount(),
		prot,
		flags | MAP_FIXED,
		mmap_fd,
		off);
	assert (!res && "Could not map anonymous region");

	ptraceCopy(pid, prot);
}

void PTImgMapEntry::mapLib(pid_t pid)
{
	int			prot, flags;

	if (	strcmp(libname, "[vsyscall]") == 0 ||
		strcmp(libname, "[vectors]") == 0)
	{
		/* the infamous syspage */
		char	*sysbuf = new char[getByteCount()];
		memcpy(sysbuf, (void*)(getBase().o), getByteCount());
		mem->addSysPage(getBase(), sysbuf, getByteCount());
		return;
	}

	if (strcmp(libname, "[stack]") == 0) {
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

	assert (mmap_fd != -1);

	int res = mem->mmap(
		mmap_base,
		mem_begin,
		getByteCount(),
		prot,
		flags | MAP_FIXED,
		mmap_fd,
		off);
	assert (!res && "Could not map library region");

	assert (mmap_base == mem_begin && "Could not map to same address");
	if (flags != MAP_SHARED)
		ptraceCopy(pid, prot);
}

void PTImgMapEntry::ptraceCopyRange(pid_t pid, guest_ptr m_beg,
	guest_ptr m_end)
{
	assert((m_beg & (sizeof(long) - 1)) == 0);
	assert((m_end & (sizeof(long) - 1)) == 0);

	guest_ptr copy_addr;

	copy_addr = m_beg;
	errno = 0;
	while (copy_addr != m_end) {
		long peek_data;
		peek_data = ptrace(PTRACE_PEEKDATA, pid, copy_addr.o, NULL);
		if (peek_data == -1 && errno) {
			fprintf(stderr,
				"Bad access: addr=%p err=%s\n", 
				(void*)copy_addr.o, strerror(errno));
		}
		assert (peek_data != -1 || errno == 0);

		if (mem->read<long>(copy_addr) != peek_data) {
			mem->write(copy_addr, peek_data);
		}
		copy_addr.o += sizeof(long);
	}
}

void PTImgMapEntry::ptraceCopy(pid_t pid, int prot)
{
	if (!(prot & PROT_READ)) return;

	if (!(prot & PROT_WRITE)) {
		int res = mem->mprotect(mmap_base, getByteCount(),
			prot | PROT_WRITE);
		assert(!res && "granting temporary write permission failed");
	}

	ptraceCopyRange(pid, mem_begin, mem_end);

	if (!(prot & PROT_WRITE)) {
		int res = mem->mprotect(mmap_base, getByteCount(), prot);
		assert(!res && "removing temporary write permission failed");
	}
}

PTImgMapEntry::PTImgMapEntry(GuestMem* in_mem, pid_t pid, const char* mapline)
: mmap_base(0)
, mmap_fd(-1)
, mem(in_mem)
{
	int		rc;

	libname[0] = '\0';
	rc = sscanf(mapline, "%p-%p %s %x %d:%d %d %s",
		(void**)&mem_begin,
		(void**)&mem_end,
		perms,
		&off,
		&t[0],
		&t[1],
		&xxx,
		libname);

	assert (rc >= 0);

	if (dump_maps) fprintf(stderr, "PTImgMapEntry: %s", mapline);

	/* now map it in */
	if (strlen(libname) <= 0) {
		mapAnon(pid);
		return;
	}

	mapLib(pid);
	if (strcmp(libname, "[stack]") == 0) {
		mem->setType(getBase(), GuestMem::Mapping::STACK);
	} else if (strcmp(libname, "[heap]") == 0) {
		mem->setType(getBase(), GuestMem::Mapping::HEAP);
	}
}

PTImgMapEntry::~PTImgMapEntry(void)
{
	if (mmap_fd) close(mmap_fd);
	if (mmap_base) mem->munmap(mmap_base, getByteCount());
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

		mapping = new PTImgMapEntry(mem, pid, line_buf);
		mappings.add(mapping);
		mem->nameMapping(mapping->getBase(), mapping->getLib());
	}
	fclose(f);
}

void GuestPTImg::slurpRegisters(pid_t pid)
{
	pt_arch->slurpRegisters();
}

void GuestPTImg::slurpBrains(pid_t pid)
{
	slurpMappings(pid);
	slurpRegisters(pid);
}

void GuestPTImg::stackTrace(
	std::ostream& os,
	const char* binname, pid_t pid,
	guest_ptr range_begin, guest_ptr range_end)
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
			(char*)NULL);
		exit(1);
	}

	  // close the write end of the pipe in the parent
	close(pipefd[1]);

	while ((bytes = read(pipefd[0], buffer, sizeof(buffer))) != 0)
		os.write(buffer, bytes);
}

void GuestPTImg::loadSymbols(void) const
{
	std::set<std::string>	mmap_fnames;
	const char		*preload_lib;

	assert (symbols == NULL && "symbols already loaded");
	symbols = new Symbols();

	/* This is a really shitty hack to force VEXLLVM_PRELOAD libraries
	 * to override default symbols. In the future, we need to support
	 * duplicate symbols */
	preload_lib = getenv("VEXLLVM_PRELOAD");
	if (preload_lib != NULL) {
		Symbols		*new_syms;
		guest_ptr	base(0);

		foreach (it, mappings.begin(), mappings.end()) {
			if ((*it)->getLib() == preload_lib) {
				base = (*it)->getBase();
				break;
			}
		}

		assert (base.o && "Could not find VEXLLVM_PRELOAD library!");
		new_syms = ElfDebug::getSyms(preload_lib, base);
		if (new_syms != NULL) {
			symbols->addSyms(new_syms);
			delete new_syms;
		}

		mmap_fnames.insert(preload_lib);
	}

	foreach (it, mappings.begin(), mappings.end()) {
		std::string	libname((*it)->getLib());
		guest_ptr	base((*it)->getBase());
		Symbols		*new_syms;

		if (libname.size() == 0)
			continue;

		/* already seen it? */
		if (mmap_fnames.count(libname) != 0)
			continue;

		/* new fname, try to load the symbols */
		new_syms = ElfDebug::getSyms(libname.c_str(), base);
		if (new_syms) {
			symbols->addSyms(new_syms);
			delete new_syms;
		}

		mmap_fnames.insert(libname);
	}
}

void GuestPTImg::setBreakpoint(pid_t pid, guest_ptr addr)
{
	if (breakpoints.count(addr))
		return;

	breakpoints[addr] = pt_arch->setBreakpoint(addr);
}


guest_ptr GuestPTImg::undoBreakpoint(pid_t pid)
{ return pt_arch->undoBreakpoint(); }

void GuestPTImg::resetBreakpoint(pid_t pid, guest_ptr addr)
{
	uint64_t	old_v;

	assert (breakpoints.count(addr) && "Resetting non-BP!");

	old_v = breakpoints[addr];
	pt_arch->resetBreakpoint(addr, old_v);
	breakpoints.erase(addr);
}

const Symbols* GuestPTImg::getSymbols(void) const
{
	if (!symbols) loadSymbols();
	return symbols;
}

const Symbols* GuestPTImg::getDynSymbols(void) const
{
	if (!dyn_symbols) loadDynSymbols();
	return dyn_symbols;
}

void GuestPTImg::loadDynSymbols(void) const
{
	Symbols	*exec_syms;

	assert (dyn_symbols == NULL && "symbols already loaded");
	dyn_symbols = new Symbols();

	/* XXX, in the future, we should look at what is in the 
	 * jump slots, for now, we rely on the symbol and hope no one
	 * is jacking the jump table */
	exec_syms = ElfDebug::getLinkageSyms(getMem(), getBinaryPath());
	if (exec_syms) {
		dyn_symbols->addSyms(exec_syms);
		delete exec_syms;
	}
}
