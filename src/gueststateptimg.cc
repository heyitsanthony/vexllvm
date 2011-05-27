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

#include "elfimg.h"

#include "gueststateptimg.h"
#include "guestcpustate.h"

using namespace llvm;

static char trap_opcode[] = { 0xcd, 0x80, 0xcc, 0x00 };

GuestStatePTImg::GuestStatePTImg(
	int argc, char *const argv[], char *const envp[])
	: child_pid(0), binary(argv[0]), steps(0), blocks(0), log_steps(false)
{
	pid_t           pid;
	int		err;
	long		old_v;

	ElfImg	*img = ElfImg::createUnlinked(argv[0]);
	assert (img != NULL && "DOES BINARY EXIST?");

	entry_pt = img->getEntryPoint();	
	delete img;

	pid = fork();
	if (pid == 0) {
		/* child */
		int     err;
		ptrace(PTRACE_TRACEME, 0, NULL, NULL);
		err = execvpe(argv[0], argv, envp);
		assert (err != -1 && "EXECVE FAILED. NO PTIMG!");
	}

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
	err = kill(pid, SIGSTOP);
	assert (err != -1);
	user_regs_struct regs;
	err = ptrace(PTRACE_GETREGS, pid, NULL, &regs);
	assert (err != -1);
	regs.rip = (uint64_t)entry_pt;
	err = ptrace(PTRACE_SETREGS, pid, NULL, &regs);

	/* hit the entry point, everything should be linked now-- load it
	 * into our context! */
	/* cleanup bp */
	err = ptrace(PTRACE_POKETEXT, pid, entry_pt, (void*)old_v);
	assert (err != -1);

	slurpBrains(pid);
	
	log_steps = getenv("VEXLLVM_LOG_STEPS") ? true : false;
	if(getenv("VEXLLVM_CROSS_CHECK")) {
		child_pid = pid;
	} else {
		/* have already ripped out the process's brains, discard it */
		ptrace(PTRACE_KILL, pid, NULL, NULL);
		wait(NULL);
	}
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
		fgets(buf, 256, f_self);
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

		fgets(line_buf, 256, f);
		mapping = new PTImgMapEntry(pid, line_buf);
		mappings.add(mapping);
	}
	fclose(f);
}

void GuestStatePTImg::slurpRegisters(pid_t pid)
{
	struct user_regs_struct	regs;
	struct user_fpregs_struct fpregs;

	int err = ptrace(PTRACE_GETREGS, pid, NULL, &regs); 
	assert(err != -1);
	err = ptrace(PTRACE_GETFPREGS, pid, NULL, &fpregs); 
	assert(err != -1);
	cpu_state->setRegs(regs, fpregs);
	// cpu_state->setTLS(new PTImgTLS((void*)regs.fs_base));
}

void GuestStatePTImg::slurpBrains(pid_t pid)
{
	slurpMappings(pid);
	slurpRegisters(pid);
}
//the basic concept here is to single step the subservient copy of the program while
//the program counter is within a specific range.  that range will correspond to the
//original range of addresses contained within the translation block.  this will allow
//things to work properly in cases where the exit from the block at the IR level 
//re-enters the block.  this is nicer than the alternative of injecting breakpoint
//instructions into the original process because it avoids the need for actually parsing
//the opcodes at this level of the code.  it's bad from the perspective of performance
//because a long running loop that could have been entirely contained within a translation
//block won't be able to fully execute without repeated syscalls to grab the process
//state
//
//a nice alternative style of doing this type of partial checking would be to run until the next
//system call.  this can happen extremely efficiently as the ptrace api provides a simple call to
//stop at that point.  identifying the errant code that causes a divergence would be tricker, but
//the caller of such a utility could track all of the involved translation blocks and produce
//a summary of the type of instructions included in the IR.  as long as the bug was an instruction
//misimplementation this would probably catch it rapidly.  this approach might be more desirable if
//we have divergent behavior that happens much later in the execution cycle.  unfortunately, there
//are probably many other malloc, getpid, gettimeofday type calls that will cause divergence
//early enough to cause the cross checking technique to fail before the speed of the mechanism really
//made an impact in how far it can check.
//
//we should probably figure out if an instruction is a syscall and mash the regs up directly
//but for now, just take the hint from the translated code
bool GuestStatePTImg::continueWithBounds(uint64_t start, uint64_t end, const VexGuestAMD64State& state)
{
	assert(child_pid);

	if(log_steps) 
		std::cerr << "RANGE: " << (void*)start << "-" << (void*)end << std::endl;
	int err;
	user_regs_struct regs;
	for(;;) {
		err = ptrace(PTRACE_GETREGS, child_pid, NULL, &regs);
		if(err < 0) {
			perror("GuestStatePTImg::continueWithBounds ptrace get registers");
			exit(1);
		}
		if(regs.rip < start || regs.rip >= end) {
			if(log_steps) 
				std::cerr << "STOPPING: " << (void*)regs.rip << std::endl;
			break;
		}
		else {
			if(log_steps) 
				std::cerr << "STEPPING: " << (void*)regs.rip << std::endl;
		}
		long res = ptrace(PTRACE_PEEKTEXT, child_pid, regs.rip, NULL);
		bool is_syscall = (res & 0xffff) == 0x050f;
		long old_rdi, old_r10;
		bool syscall_restore_rdi_r10 = false;
		if(is_syscall) {
			if(regs.rax == SYS_brk) {
				regs.rax = -1;
				regs.rip += 2;
				regs.rcx = regs.r11 = 0;
				err = ptrace(PTRACE_SETREGS, child_pid, NULL, &regs);
				if(err < 0) {
					perror("GuestStatePTImg::continueWithBounds "
						"ptrace set registers pre-brk");
					exit(1);
				}
				continue;
			}
			if(regs.rax == SYS_getpid) {
				regs.rax = getpid();
				regs.rip += 2;
				regs.rcx = regs.r11 = 0;
				err = ptrace(PTRACE_SETREGS, child_pid, NULL, &regs);
				if(err < 0) {
					perror("GuestStatePTImg::continueWithBounds "
						"ptrace set registers pre-getpid");
					exit(1);
				}
				continue;
			}
			if(regs.rax == SYS_mmap) {
				syscall_restore_rdi_r10 = true;
				old_rdi = regs.rdi;
				old_r10 = regs.r10;
				regs.rdi = state.guest_RAX;
				regs.r10 |= MAP_FIXED;
				err = ptrace(PTRACE_SETREGS, child_pid, NULL, &regs);
				if(err < 0) {
					perror("GuestStatePTImg::continueWithBounds "
						"ptrace set registers pre-mmap");
					exit(1);
				}
			}
		}

		++steps;
		err = ptrace(PTRACE_SINGLESTEP, child_pid, NULL, NULL);
		if(err < 0) {
			perror("GuestStatePTImg::continueWithBounds ptrace single step");
			exit(1);
		}
		//note that in this scenario we don't care whether or not
		//a syscall was the source of the trap (two traps are produced
		//per syscall, pre+post).
		wait(NULL);
		if(is_syscall) {
			err = ptrace(PTRACE_GETREGS, child_pid, NULL, &regs);
			if(err < 0) {
				perror("GuestStatePTImg::continueWithBounds ptrace post syscall refresh registers");
				exit(1);
			}
			if(syscall_restore_rdi_r10) {
				regs.r10 = old_r10;
				regs.rdi = old_rdi;
			}
			//kernel clobbers these, we are assuming that the generated code, causes
			regs.rcx = regs.r11 = 0;
			err = ptrace(PTRACE_SETREGS, child_pid, NULL, &regs);
			if(err < 0) {
				perror("GuestStatePTImg::continueWithBounds ptrace set registers after syscall");
				exit(1);
			}
		}
	}
	
	++blocks;
	user_fpregs_struct fpregs;
	err = ptrace(PTRACE_GETFPREGS, child_pid, NULL, &fpregs);
	if(err < 0) {
		perror("GuestStatePTImg::continueWithBounds  ptrace get fp registers");
		exit(1);
	}
	
	bool x86_fail = 
		(state.guest_RAX ^ regs.rax) |
		(state.guest_RCX ^ regs.rcx) |
		(state.guest_RDX ^ regs.rdx) |
		(state.guest_RBX ^ regs.rbx) |
		(state.guest_RSP ^ regs.rsp) |
		(state.guest_RBP ^ regs.rbp) |
		(state.guest_RSI ^ regs.rsi) |
		(state.guest_RDI ^ regs.rdi) |
		(state.guest_R8  ^ regs.r8) |
		(state.guest_R9  ^ regs.r9) |
		(state.guest_R10 ^ regs.r10) |
		(state.guest_R11 ^ regs.r11) |
		(state.guest_R12 ^ regs.r12) |
		(state.guest_R13 ^ regs.r13) |
		(state.guest_R14 ^ regs.r14) |
		(state.guest_R15 ^ regs.r15) |
		(state.guest_RIP ^ regs.rip);

	//TODO: some kind of eflags, checking but i don't yet understand this
	//mess of broken apart state.
	//
	// /* 4-word thunk used to calculate O S Z A C P flags. */
	// /* 128 */ ULong  guest_CC_OP;
	// /* 136 */ ULong  guest_CC_DEP1;
	// /* 144 */ ULong  guest_CC_DEP2;
	// /* 152 */ ULong  guest_CC_NDEP;
	// /* The D flag is stored here, encoded as either -1 or +1 */
	// /* 160 */ ULong  guest_DFLAG;
	// /* 168 */ ULong  guest_RIP;
	// /* Bit 18 (AC) of eflags stored here, as either 0 or 1. */
	// /* ... */ ULong  guest_ACFLAG;
	// /* Bit 21 (ID) of eflags stored here, as either 0 or 1. */
	
	//TODO: segments? but valgrind/vex seems to not really fully handle them, how sneaky
	bool seg_fail = (regs.fs_base ^ state.guest_FS_ZERO);

	//TODO: what is this for? well besides the obvious
	// /* 192 */ULong guest_SSEROUND;

	bool sse_ok = !memcmp(&state.guest_XMM0, &fpregs.xmm_space[0], sizeof(fpregs.xmm_space));

	//TODO: check the top pointer of the floating point stack..
	// /* FPU */
	// /* 456 */UInt  guest_FTOP;
	//FPTAG?

	//TODO: this is surely wrong, the sizes don't even match...
	bool x87_ok = !memcmp(&state.guest_FPREG[0], &fpregs.st_space[0], sizeof(state.guest_FPREG));

	//TODO: what are these?
	// /* 536 */ ULong guest_FPROUND;
	// /* 544 */ ULong guest_FC3210;

	//probably not TODO: more stuff that is likely unneeded
	// /* 552 */ UInt  guest_EMWARN;
	// ULong guest_TISTART;
	// ULong guest_TILEN;
	// ULong guest_NRADDR;
	// ULong guest_SC_CLASS;
	// ULong guest_GS_0x60;
	// ULong guest_IP_AT_SYSCALL;
	// ULong padding;

	return !x86_fail && x87_ok & sse_ok && !seg_fail;
}
void GuestStatePTImg::stackTraceSubservient(std::ostream& os) 
{
	assert(child_pid);
	
	std::ostringstream pid_string;
	pid_string << child_pid;
	
	int pipefd[2];
	pipe(pipefd);
	
	kill(child_pid, SIGSTOP);
	ptrace(PTRACE_DETACH, child_pid, NULL, NULL);
	
	if (!fork()) {
		close(pipefd[0]);    // close reading end in the child
		dup2(pipefd[1], 1);  // send stdout to the pipe
		dup2(pipefd[1], 2);  // send stderr to the pipe
		close(pipefd[1]);    // this descriptor is no longer needed

		execl("/usr/bin/gdb", 
			"/usr/bin/gdb",
			"--batch",
			binary.c_str(),
			pid_string.str().c_str(),
			"--eval-command",
			"thread apply all bt",
			"--eval-command",
			"disass",
			"--eval-command",
			"kill",
			NULL
		);
		exit(1);
	}
	else
	{
		char buffer[1024];
		close(pipefd[1]);  // close the write end of the pipe in the parent
		int bytes;
		while ((bytes = read(pipefd[0], buffer, sizeof(buffer))) != 0) {
			os.write(buffer, bytes);
		}	
	}
}

void GuestStatePTImg::printSubservient(std::ostream& os, const VexGuestAMD64State* ref) 
{
	assert(child_pid);

	int err;
	user_regs_struct regs;
	err = ptrace(PTRACE_GETREGS, child_pid, NULL, &regs);
	if(err < 0) {
		perror("GuestStatePTImg::printState ptrace get registers");
		exit(1);
	}
	user_fpregs_struct fpregs;
	err = ptrace(PTRACE_GETFPREGS, child_pid, NULL, &fpregs);
	if(err < 0) {
		perror("GuestStatePTImg::printState ptrace get fp registers");
		exit(1);
	}
	
	if(ref && regs.rip != ref->guest_RIP) os << "***";
	os << "rip: " << (void*)regs.rip << std::endl;
	if(ref && regs.rax != ref->guest_RAX) os << "***";
	os << "rax: " << (void*)regs.rax << std::endl;
	if(ref && regs.rbx != ref->guest_RBX) os << "***";
	os << "rbx: " << (void*)regs.rbx << std::endl;
	if(ref && regs.rcx != ref->guest_RCX) os << "***";
	os << "rcx: " << (void*)regs.rcx << std::endl;
	if(ref && regs.rdx != ref->guest_RDX) os << "***";
	os << "rdx: " << (void*)regs.rdx << std::endl;
	if(ref && regs.rsp != ref->guest_RSP) os << "***";
	os << "rsp: " << (void*)regs.rsp << std::endl;
	if(ref && regs.rbp != ref->guest_RBP) os << "***";
	os << "rbp: " << (void*)regs.rbp << std::endl;
	if(ref && regs.rdi != ref->guest_RDI) os << "***";
	os << "rdi: " << (void*)regs.rdi << std::endl;
	if(ref && regs.rsi != ref->guest_RSI) os << "***";
	os << "rsi: " << (void*)regs.rsi << std::endl;
	if(ref && regs.r8 != ref->guest_R8) os << "***";
	os << "r8: " << (void*)regs.r8 << std::endl;
	if(ref && regs.r9 != ref->guest_R9) os << "***";
	os << "r9: " << (void*)regs.r9 << std::endl;
	if(ref && regs.r10 != ref->guest_R10) os << "***";
	os << "r10: " << (void*)regs.r10 << std::endl;
	if(ref && regs.r11 != ref->guest_R11) os << "***";
	os << "r11: " << (void*)regs.r11 << std::endl;
	if(ref && regs.r12 != ref->guest_R12) os << "***";
	os << "r12: " << (void*)regs.r12 << std::endl;
	if(ref && regs.r13 != ref->guest_R13) os << "***";
	os << "r13: " << (void*)regs.r13 << std::endl;
	if(ref && regs.r14 != ref->guest_R14) os << "***";
	os << "r14: " << (void*)regs.r14 << std::endl;
	if(ref && regs.r15 != ref->guest_R15) os << "***";
	os << "r15: " << (void*)regs.r15 << std::endl;

	//TODO: some kind of eflags, checking but i don't yet understand this
	//mess of broken apart state.
	//
	// /* 4-word thunk used to calculate O S Z A C P flags. */
	// /* 128 */ ULong  guest_CC_OP;
	// /* 136 */ ULong  guest_CC_DEP1;
	// /* 144 */ ULong  guest_CC_DEP2;
	// /* 152 */ ULong  guest_CC_NDEP;
	// /* The D flag is stored here, encoded as either -1 or +1 */
	// /* 160 */ ULong  guest_DFLAG;
	// /* 168 */ ULong  guest_RIP;
	// /* Bit 18 (AC) of eflags stored here, as either 0 or 1. */
	// /* ... */ ULong  guest_ACFLAG;
	// /* Bit 21 (ID) of eflags stored here, as either 0 or 1. */
	
	//TODO: segments? but valgrind/vex seems to not really fully handle them, how sneaky
	if(ref && regs.fs_base != ref->guest_FS_ZERO) os << "***";
	os << "fs_base: " << (void*)regs.fs_base << std::endl;

	//TODO: what is this for? well besides the obvious
	// /* 192 */ULong guest_SSEROUND;

	for(int i = 0; i < 16; ++i) {
		if(ref && memcmp(&fpregs.xmm_space[i * 4], 
			&(&ref->guest_XMM0)[i], sizeof(ref->guest_XMM0)))
			os << "***";
		os << "xmm" << i << ": " 
			<< *(void**)&fpregs.xmm_space[i * 4 + 2] << "|"
			<< *(void**)&fpregs.xmm_space[i * 4 + 0] << std::endl;
	}

	//TODO: check the top pointer of the floating point stack..
	// /* FPU */
	// /* 456 */UInt  guest_FTOP;
	//FPTAG?

	for(int i = 0; i < 8; ++i) {
		if(ref && memcmp(&fpregs.st_space[i * 4], 
			&ref->guest_FPREG[i], sizeof(ref->guest_FPREG[0])))
			os << "***";
		os << "st" << i << ": " 
			<< *(void**)&fpregs.st_space[i * 4 + 2] << "|"
			<< *(void**)&fpregs.st_space[i * 4 + 0] << std::endl;
	}
}
void GuestStatePTImg::printTraceStats(std::ostream& os) 
{
	os << "Traced " << blocks << " blocks, stepped " << steps << " instructions" << std::endl;
}

