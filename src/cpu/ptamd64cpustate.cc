#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <string.h>
#include "cpu/ptamd64cpustate.h"

#define OPCODE_SYSCALL	0x050f

struct pt_regs {
	struct user_regs_struct		regs;
	struct user_fpregs_struct	fpregs;
};

PTAMD64CPUState::PTAMD64CPUState(pid_t in_pid)
	: PTCPUState(in_pid)
{
	state_byte_c = sizeof(struct pt_regs);
	state_data = new uint8_t[state_byte_c+1];
	memset(state_data, 0, state_byte_c+1);
}

PTAMD64CPUState::~PTAMD64CPUState()
{
	delete [] state_data;
}

void PTAMD64CPUState::loadRegs(void)
{
	struct pt_regs	&pregs = *((struct pt_regs*)state_data);

	reloadRegs();

	/*** linux is busted, quirks ahead ***/
	/* XXX: I think this is still kind of busted.. need tests.. */

	/* this is kind of a stupid hack but I don't know an easier way
	 * to get orig_rax */
#define RAX_IN_SYSCALL	0xffffffda
	if (	pregs.regs.orig_rax != ~((uint64_t)0) &&
		(pregs.regs.rax & 0xffffffff) == RAX_IN_SYSCALL)
	{
		pregs.regs.rax = pregs.regs.orig_rax;
		if (pregs.regs.fs_base == 0) pregs.regs.fs_base = 0xdead;
	}
}

guest_ptr PTAMD64CPUState::undoBreakpoint(void)
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
	return guest_ptr(regs.rip);
}

long PTAMD64CPUState::setBreakpoint(guest_ptr addr)
{
	uint64_t		old_v, new_v;
	int			err;

	old_v = ptrace(PTRACE_PEEKTEXT, pid, addr.o, NULL);
	new_v = old_v & ~0xff;
	new_v |= 0xcc;

	err = ptrace(PTRACE_POKETEXT, pid, addr.o, new_v);
	assert (err != -1 && "Failed to set breakpoint");

	return old_v;
}

guest_ptr PTAMD64CPUState::getPC(void) const { return guest_ptr(getRegs().rip); }
guest_ptr PTAMD64CPUState::getStackPtr(void) const {return guest_ptr(getRegs().rsp);}
uintptr_t PTAMD64CPUState::getSysCallResult() const { return getRegs().rax; }
void PTAMD64CPUState::setStackPtr(guest_ptr p) { getRegs().rsp = p.o; }
void PTAMD64CPUState::setPC(guest_ptr p) { getRegs().rip = p.o; }


void PTAMD64CPUState::setRegs(const user_regs_struct& regs)
{
	struct pt_regs	*s_ptreg = (struct pt_regs*)state_data;
	int		err;

	err = ptrace(PTRACE_SETREGS, pid, NULL, &regs);
	if (err < 0) {
		fprintf(stderr, "DIDN'T TAKE?? pid=%d\n", pid);
		perror("PTAMD64CPUState::setregs");
		abort();
		exit(1);
	}

	if (&regs != &s_ptreg->regs) {
		s_ptreg->regs = regs;
	}

	recent_shadow = true;
}

struct user_regs_struct& PTAMD64CPUState::getRegs(void) const
{
	struct pt_regs	*s_ptreg = (struct pt_regs*)state_data;
	if (!recent_shadow) {
		reloadRegs();
	}
	return s_ptreg->regs;
}

struct user_fpregs_struct& PTAMD64CPUState::getFPRegs(void) const
{
	struct pt_regs	*s_ptreg = (struct pt_regs*)state_data;
	if (!recent_shadow) {
		reloadRegs();
	}
	return s_ptreg->fpregs;
}

void PTAMD64CPUState::reloadRegs(void) const
{
	pt_regs	&pregs = *(struct pt_regs*)state_data;
	int	err[2];

	err[0] = ptrace(PTRACE_GETREGS, pid, NULL, &pregs.regs);
	err[1] = ptrace(PTRACE_GETFPREGS, pid, NULL, &pregs.fpregs);
	recent_shadow = true;

	if (err[0] >= 0 && err[1] >= 0)
		return;

	perror("PTAMD64CPUState::getRegs");

	/* The politics of failure have failed.
	 * It is time to make them work again.
	 * (this is temporary nonsense to get
	 *  /usr/bin/make xchk tests not to hang). */
	waitpid(pid, NULL, 0);
	kill(pid, SIGABRT);
	raise(SIGKILL);
	_exit(-1);
	abort();
}

void PTAMD64CPUState::ignoreSysCall(void)
{
	user_regs_struct	*regs;

	regs = &getRegs();
	regs->rip += 2;
	/* kernel clobbers these */
	regs->rcx = regs->r11 = 0;
	setRegs(*regs);
	return;
}


uint64_t PTAMD64CPUState::dispatchSysCall(const SyscallParams& sp, int& wss)
{
	struct user_regs_struct	old_regs, new_regs;
	uint64_t		ret;
	uint64_t		old_op;

	old_regs = getRegs();

	/* patch in system call opcode */
	old_op = ptrace(PTRACE_PEEKDATA, pid, old_regs.rip, NULL);
	ptrace(PTRACE_POKEDATA, pid, old_regs.rip, (void*)OPCODE_SYSCALL);

	new_regs = old_regs;
	new_regs.rax = sp.getSyscall();
	new_regs.rdi = sp.getArg(0);
	new_regs.rsi = sp.getArg(1);
	new_regs.rdx = sp.getArg(2);
	new_regs.r10 = sp.getArg(3);
	new_regs.r8 = sp.getArg(4);
	new_regs.r9 =  sp.getArg(5);
	ptrace(PTRACE_SETREGS, pid, NULL, &new_regs);

	wss = waitForSingleStep();

	ret = getSysCallResult();

	/* reload old state */
	ptrace(PTRACE_POKEDATA, pid, old_regs.rip, (void*)old_op);
	setRegs(old_regs);

	return ret;
}
