#include <stdio.h>
#include <assert.h>
#include <sys/ptrace.h>
#include <asm/ptrace.h>
#include "cpu/ptimgarm.h"
#include "cpu/armcpustate.h"

extern "C" {
#include "valgrind/libvex_guest_arm.h"
}

PTImgARM::PTImgARM(GuestPTImg* gs, int in_pid)
: PTImgArch(gs, in_pid)
{}

PTImgARM::~PTImgARM() {}

bool PTImgARM::isMatch(void) const { assert (0 == 1 && "STUB"); }
bool PTImgARM::doStep(guest_ptr start, guest_ptr end, bool& hit_syscall)
{ assert (0 == 1 && "STUB"); }

uintptr_t PTImgARM::getSysCallResult() const { assert (0 == 1 && "STUB"); }

const VexGuestARMState& PTImgARM::getVexState(void) const
{ return *((const VexGuestARMState*)gs->getCPUState()->getStateData()); }

bool PTImgARM::isRegMismatch() const { assert (0 == 1 && "STUB"); }
void PTImgARM::printFPRegs(std::ostream&) const { assert (0 == 1 && "STUB"); }
void PTImgARM::printUserRegs(std::ostream&) const { assert (0 == 1 && "STUB"); }
guest_ptr PTImgARM::getStackPtr() const
{ return guest_ptr(getVexState().guest_R13); }

void PTImgARM::slurpRegisters(void)
{

	ARMCPUState			*arm_cpu_state;
	int				err;
	struct user_regs		regs;
	uint8_t				vfpregs[ARM_VFPREGS_SIZE];
	void				*thread_area;

	err = ptrace((__ptrace_request)PTRACE_GETREGS, child_pid, NULL, &regs);
	assert(err != -1);
	err = ptrace(
		(__ptrace_request)PTRACE_GETVFPREGS,
		child_pid, NULL, &vfpregs);
	assert(err != -1);

	err = ptrace(
		(__ptrace_request)PTRACE_GET_THREAD_AREA,
		child_pid, NULL, &thread_area);
	assert(err != -1);

	arm_cpu_state = (ARMCPUState*)gs->getCPUState();
	arm_cpu_state->setRegs(&regs, vfpregs, thread_area);
}

void PTImgARM::stepSysCall(SyscallsMarshalled*) { assert (0 == 1 && "STUB"); }

/* from arch/arm/kernel/ptrace.c */
#define ARM_BKPT	0xe7f001f0UL	// bkpt instruction?
//#define ARM_BKPT	0xf001f0e7UL	// am I retarded on endian?
//#define ARM_BKPT	0xe7ffdefeUL	// undefined op
#define THUMB_BKPT	0xde01UL

long int PTImgARM::setBreakpoint(guest_ptr addr)
{
	uint32_t		old_v;
	uint32_t		new_v;
	int			err;

	old_v = ptrace(PTRACE_PEEKTEXT, child_pid, addr.o & ~3UL, NULL);
	if (addr.o & 1) {
		/* 16-bit THUMB */
		if (addr.o & 2) {
			new_v = (old_v & 0xffff) | (THUMB_BKPT << 16);
		} else {
			new_v = (old_v & 0xffff0000) | THUMB_BKPT;
		}
	} else {
		new_v = ARM_BKPT;
	}

	err = ptrace(PTRACE_POKETEXT, child_pid, addr.o & ~3UL, new_v);
	assert (err != -1 && "Failed to set breakpoint");

	fprintf(stderr, "setting breakpoint at %p\n", (void*)addr.o);

	struct user_regs	regs;
	ptrace((__ptrace_request)PTRACE_GETREGS, child_pid, NULL, &regs);

	for (int i = 0; i < 18; i++)
		fprintf(stderr, "R[%02d]: %p\n", i,  regs.uregs[i]);


	return old_v;
}

void PTImgARM::resetBreakpoint(guest_ptr addr, long v)
{
	int err;
	err = ptrace(PTRACE_POKETEXT, child_pid, addr.o & ~3UL, v);
	assert (err != -1 && "Failed to reset breakpoint");
}

#define UREG_IDX_PC	15
#define UREG_IDX_CPSR	16
#define CPSR_FL_THUMB	(1 << 5)

guest_ptr PTImgARM::undoBreakpoint()
{
	struct user_regs	regs;
	int			err;

	/* should be halted on our trapcode. need to set rip prior to
	 * trapcode addr */
	err = ptrace((__ptrace_request)PTRACE_GETREGS, child_pid, NULL, &regs);
	assert (err != -1);

	/* no need backtrack before breakpoint.. */

	/* ptrace will mask out the thumb bit here, so we need to
	 * manually patch it back that we patch the right address */
	if (CPSR_FL_THUMB & regs.uregs[UREG_IDX_CPSR])
		return guest_ptr(regs.uregs[15] | 1);

	return guest_ptr(regs.uregs[15]);
}

guest_ptr PTImgARM::getPC() { assert (0 == 1 && "STUB"); }
GuestPTImg::FixupDir PTImgARM::canFixup(
	const std::vector<std::pair<guest_ptr, unsigned char> >&,
	bool has_memlog) const { assert (0 == 1 && "STUB"); }
bool PTImgARM::breakpointSysCalls(guest_ptr, guest_ptr) { assert (0 == 1 && "STUB"); }
void PTImgARM::revokeRegs() { assert (0 == 1 && "STUB"); }


/* from linux sources.. arch/arm/include/uapi/asm/ptrace.h */
#define ARM_ORIG_r0 uregs[17]
#define OPCODE_SVC	0xef000000
void PTImgARM::fixupRegsPreSyscall(int pid)
{
	struct user_regs	r;
	int			err;
	uint32_t		*x;
	GuestCPUState		*cpu;

	cpu = gs->getCPUState();
	x = (uint32_t*)gs->getMem()->getHostPtr(cpu->getPC());
	assert (x[-1] == OPCODE_SVC);
	
	err = ptrace(__ptrace_request(PTRACE_GETREGS), pid, NULL, &r);
	assert (err == 0);

	/* put PC back on svc instruction */
	cpu->setPC(guest_ptr(cpu->getPC()-4));

	/* restore syscall R0 */
	gs->setSyscallResult(r.ARM_ORIG_r0);
}
