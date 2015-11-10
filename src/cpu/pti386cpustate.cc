#include <sys/ptrace.h>
#include <asm/ptrace-abi.h>
#include <string.h>
#include "cpu/pti386cpustate.h"

struct x86_user_fpxregs
{
  unsigned short int cwd;
  unsigned short int swd;
  unsigned short int twd;
  unsigned short int fop;
  long int fip;
  long int fcs;
  long int foo;
  long int fos;
  long int mxcsr;
  long int reserved;
  long int st_space[32];   /* 8*16 bytes for each FP-reg = 128 bytes */
  long int xmm_space[32];  /* 8*16 bytes for each XMM-reg = 128 bytes */
  long int padding[56];
};

/* HAHA TOO BAD I CAN'T REUSE A HEADER. */
struct x86_user_regs
{
  long int ebx;
  long int ecx;
  long int edx;
  long int esi;
  long int edi;
  long int ebp;
  long int eax;
  long int xds;
  long int xes;
  long int xfs;
  long int xgs;
  long int orig_eax;
  long int eip;
  long int xcs;
  long int eflags;
  long int esp;
  long int xss;
};

struct pt_regs
{
	struct user_regs_struct		regs;
	struct user_fpregs_struct	fpregs;
};

#define GET_PTREGS()	((struct pt_regs*)state_data)

PTI386CPUState::PTI386CPUState(pid_t in_pid)
	: PTCPUState(in_pid)
{
	state_byte_c = sizeof(struct pt_regs);
	state_data = new uint8_t[state_byte_c+1];
	memset(state_data, 0, state_byte_c+1);
}

PTI386CPUState::~PTI386CPUState()
{
	delete [] state_data;
}

guest_ptr PTI386CPUState::undoBreakpoint()
{
	// struct x86_user_regs	regs;
	auto	pr = GET_PTREGS();
	int	err;

	/* should be halted on our trapcode. need to set rip prior to
	 * trapcode addr */
	err = ptrace((__ptrace_request)PTRACE_GETREGS, pid, NULL, &pr->regs);
	assert (err != -1);

	pr->regs.rip--; /* backtrack before int3 opcode */
	err = ptrace((__ptrace_request)PTRACE_SETREGS, pid, NULL, &pr->regs);

	/* run again w/out reseting BP and you'll end up back here.. */
	return guest_ptr(getRegs().rip);
}

long int PTI386CPUState::setBreakpoint(guest_ptr addr)
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

guest_ptr PTI386CPUState::getStackPtr() const { return guest_ptr(getRegs().rsp); }

void PTI386CPUState::loadRegs(void) { reloadRegs(); }

void PTI386CPUState::reloadRegs(void) const
{
	int				err;
//	struct x86_user_regs		regs;
//	struct x86_user_fpxregs		fpregs;
	auto	pr = GET_PTREGS();

	err = ptrace((__ptrace_request)PTRACE_GETREGS, pid, NULL, &pr->regs);
	assert(err != -1);

	err = ptrace((__ptrace_request)PTRACE_GETFPREGS, pid, NULL, &pr->fpregs);
	assert(err != -1);

	recent_shadow = true;
}

struct user_regs_struct& PTI386CPUState::getRegs(void) const {
	if (!recent_shadow) reloadRegs();
	return GET_PTREGS()->regs;
}
struct user_fpregs_struct& PTI386CPUState::getFPRegs(void) const {
	if (!recent_shadow) reloadRegs();
	return GET_PTREGS()->fpregs;
}
void PTI386CPUState::setRegs(const user_regs_struct& regs) {
	GET_PTREGS()->regs = regs;
}

