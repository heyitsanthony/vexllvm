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

#include "elfimg.h"

#include "gueststateptimg.h"
#include "guestcpustate.h"

using namespace llvm;

static char trap_opcode[] = { 0xcd, 0x80, 0xcc, 0x00 };

GuestStatePTImg::GuestStatePTImg(
	int argc, char *const argv[], char *const envp[])
{
	ElfImg		*img;
	pid_t		slurped_pid;
	
	img = ElfImg::createUnlinked(argv[0]);
	assert (img != NULL && "DOES BINARY EXIST?");

	entry_pt = img->getEntryPoint();	
	delete img;

	slurped_pid = createSlurpedChild(argc, argv, envp);
	handleChild(slurped_pid);
}

void GuestStatePTImg::handleChild(pid_t pid)
{
	ptrace(PTRACE_KILL, pid, NULL, NULL);
	wait(NULL);
}

pid_t GuestStatePTImg::createSlurpedChild(
	int argc, char *const argv[], char *const envp[])
{
	int		err;
	pid_t		pid;
	uint64_t	old_v;

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
	wait(NULL);

	/* Trapped the process on execve-- binary is loaded, but not linked */
	/* overwrite entry with BP. */
	old_v = ptrace(PTRACE_PEEKTEXT, pid, entry_pt, NULL);
	err = ptrace(PTRACE_POKETEXT, pid, entry_pt, trap_opcode);
	assert (err != -1);

	/* go until child hits entry point */
	err = ptrace(PTRACE_CONT, pid, NULL, NULL);
	assert (err != -1);
	wait(NULL);
	
	//stop the process and reset the program counter before repatching
#if 0
	err = kill(pid, SIGSTOP);
	assert (err != -1);
	user_regs_struct regs;
	err = ptrace(PTRACE_GETREGS, pid, NULL, &regs);
	assert (err != -1);
	err = ptrace(PTRACE_SETREGS, pid, NULL, &regs);
#endif

	/* hit the entry point, everything should be linked now-- load it
	 * into our context! */
	/* cleanup bp */
	err = ptrace(PTRACE_POKETEXT, pid, entry_pt, (void*)old_v);
	assert (err != -1);

	slurpBrains(pid);
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

	if (strcmp(libname, "[vsyscall]") == 0) return;

	mmap_fd = open(libname, O_RDONLY);
	if (mmap_fd == -1) {
		struct stat	s;
		int		rc;

		rc = stat(libname, &s);
		assert (rc == -1);
	
		mapAnon(pid);
		return;
	}

	prot = getProt();
//	if (prot & PROT_EXEC) flags = MAP_SHARED;
//	else flags = MAP_PRIVATE;
	flags = MAP_PRIVATE;

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

void PTImgMapEntry::ptraceCopy(pid_t pid, int prot)
{
	long			*copy_addr;

	if (!(prot & PROT_READ)) return;

	if (!(prot & PROT_WRITE))
		mprotect(mmap_base, getByteCount(), prot | PROT_WRITE);

	copy_addr = (long*)mem_begin;
	errno = 0;
	while (copy_addr != mem_end) {
		long	peek_data;
		peek_data = ptrace(PTRACE_PEEKDATA, pid, copy_addr, NULL);
		assert (peek_data != -1 || errno == 0);
		if (*copy_addr != peek_data) {
			*copy_addr = peek_data;
		}

		copy_addr++;
	}

	if (!(prot & PROT_WRITE))
		mprotect(mmap_base, getByteCount(), prot);
}

PTImgMapEntry::PTImgMapEntry(pid_t pid, const char* mapline)
 : mmap_base(NULL), mmap_fd(-1)
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

//	fprintf(stderr, "MAPPING: %s", mapline);
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

void GuestStatePTImg::dumpSelfMap(void) const
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

void GuestStatePTImg::slurpMappings(pid_t pid)
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

void GuestStatePTImg::slurpRegisters(pid_t pid)
{
#if 0
	int				err;
	struct user_regs_struct		regs;
	struct user_fpregs_struct	fpregs;

	err = ptrace(PTRACE_GETREGS, pid, NULL, &regs); 
	assert(err != -1);
	err = ptrace(PTRACE_GETFPREGS, pid, NULL, &fpregs); 
	assert(err != -1);

	cpu_state->setRegs(regs, fpregs);
	cpu_state->setTLS(new PTImgTLS((void*)regs.fs_base));
#endif
	struct user_regs_struct regs;

	ptrace(PTRACE_GETREGS, pid, NULL, &regs); 
	cpu_state->setTLS(new PTImgTLS((void*)regs.fs_base));
	cpu_state->setStackPtr((void*)regs.rsp);
	cpu_state->setFuncArg(regs.rdx, 2);
}

void GuestStatePTImg::slurpBrains(pid_t pid)
{
	slurpMappings(pid);
	slurpRegisters(pid);
}
