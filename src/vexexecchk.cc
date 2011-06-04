#include <iostream>
#include <stdlib.h>
#include <stdio.h>


#include "vexxlate.h"
#include "vexsb.h"

#include "guestcpustate.h"
#include "ptimgchk.h"
#include "syscallsmarshalled.h"
#include "vexexecchk.h"

VexExecChk::VexExecChk(PTImgChk* gs)
: VexExec(gs),
  hit_syscall(false), is_deferred(false)
{
	cross_check = (getGuestState()) ? gs : NULL;
	if (!cross_check) return;

	sc_marshall = new SyscallsMarshalled();
	delete sc;
	sc = sc_marshall;
}

/* ensures that shadow process's state <= llvm process's state */
uint64_t VexExecChk::doVexSB(VexSB* vsb)
{
	const VexGuestAMD64State*	state;
	bool				new_ip_in_bounds;
	uint64_t			new_ip;

	state = (const VexGuestAMD64State*)gs->getCPUState()->getStateData();

	/* if we're not deferred we know we're synced up already.
	 * this fact to be paranoid (make sure the states are ==)*/
	if (!is_deferred) {
		if (!cross_check->isMatch(*state)) {
			fprintf(stderr, "MISMATCH PRIOR TO doVexSB. HOW??\n");
			dumpSubservient(vsb);
		}
	}

	new_ip = VexExec::doVexSB(vsb);

	new_ip_in_bounds = 	new_ip >= vsb->getGuestAddr() &&
				new_ip < vsb->getEndAddr();

	if (is_deferred && new_ip_in_bounds) {
		/* we defer until we know for sure we're out of the SB */
		goto done;
	} else if (!is_deferred && !new_ip_in_bounds) {
		/* new ip is outside prev ip's vsb...
		 * valgrind does some nasty hacks here where it loop
		 * unrolls instructions so two IMarks can match.
		 * However, if it jumps out of a basic block, you're safe
		 * to step through the entire bounds.
		 * */
		cross_check->stepThroughBounds(
			vsb->getGuestAddr(),
			vsb->getEndAddr(),
			*state);
		is_deferred = false;
		if (vsb->isSyscall()) {
			hit_syscall = true;
			goto done;
		}

		goto states_should_be_equal;
	} else if (is_deferred && !new_ip_in_bounds) {
		/* can step out of the block now we call this a "resume" */
		cross_check->stepThroughBounds(
			deferred_bound_start, deferred_bound_end, *state);
		is_deferred = false;
		deferred_bound_start = 0;
		deferred_bound_end = 0;
		goto states_should_be_equal;
	} else if (new_ip_in_bounds) {
		 /* Nasty VexIR trivia. A SB can:
		  * 	0x123: stmt
		  * 	0x124: stmt
		  * 	0x125: if (whatever-is-false) goto whereever;
		  * 	0x123: stmt
		  * 	0x124: stmt
		  * 	0x125: if (whatever-is-false) goto whereever;
		  * 	0x126: goto 0x123
		  *
		  * 	So, a VexSB run will see 0x123 twice before terminating
		  *	but the ptrace will only see it once with continueForward.
		  *
		  *	Instead analyzing the vsb for this stuff,
		  *	record bounds and defer shadow execution until
		  *	next IP jumps out of the bounds.
		  */
		is_deferred = true;
		deferred_bound_start = vsb->getGuestAddr();
		deferred_bound_end = vsb->getEndAddr();
		goto done;
	}
	
states_should_be_equal:
	verifyBlockRun(vsb);
done:
	return new_ip;
}

/* check, fix, die */
void VexExecChk::verifyBlockRun(VexSB* vsb)
{
	bool fixed;

	if (cross_check->isMatch(
		*(const VexGuestAMD64State*)gs->getCPUState()->getStateData()))
	{
		 return;
	}

	fixed = cross_check->fixup(
		(void*)vsb->getGuestAddr(), 
		(void*)vsb->getEndAddr());
	if (fixed) return;

	fprintf(stderr, "MISMATCH: END OF BLOCK. FIND NEW EMU BUG.\n");
	dumpSubservient(vsb);
}

void VexExecChk::stepSysCall(VexSB* vsb)
{
	VexGuestAMD64State* state;

	state = (VexGuestAMD64State*)gs->getCPUState()->getStateData();
	cross_check->stepSysCall(sc_marshall, *state);

	state->guest_RCX = 0;
	state->guest_R11 = 0;
}

void VexExecChk::doSysCall(VexSB* vsb)
{
	VexGuestAMD64State* state;

	state = (VexGuestAMD64State*)gs->getCPUState()->getStateData();
	assert (hit_syscall && !is_deferred);

	/* recall: shadow process <= vexllvm. hence, we must call the 
	 * syscall for vexllvm prior to the syscall for the shadow process.
	 * this lets us to fixups to match vexllvm's calls. */
	VexExec::doSysCall(vsb);

	stepSysCall(vsb);

	/* now both should be equal and at the instruction immediately following
	 * the breaking syscall */
	if (!cross_check->isMatch(*state)) {
		fprintf(stderr, "MISMATCH: END OF SYSCALL. SYSEMU BUG.\n");
		dumpSubservient(vsb);
	}

	hit_syscall = false;
}

void VexExecChk::dumpSubservient(VexSB* vsb)
{
	const VexGuestAMD64State* state;
	
	state = (const VexGuestAMD64State*)gs->getCPUState()->getStateData();
	if (vsb != NULL) {
		std::cerr << 
			"found divergence running block @ " <<
			(void*)vsb->getGuestAddr() << std::endl;

		std::cerr << 
			"original block end was @ " <<
			(void*)vsb->getEndAddr() << std::endl;
	}

	cross_check->printTraceStats(std::cerr);
	std::cerr << "PTRACE state" << std::endl;
	cross_check->printSubservient(std::cerr, *state);
	std::cerr << "VEXLLVM state" << std::endl;
	gs->print(std::cerr);

	if (vsb != NULL) {
		std::cerr << "VEX IR" << std::endl;
		vsb->print(std::cerr);
	}

	std::cerr << "PTRACE stack" << std::endl;
	cross_check->stackTraceSubservient(
		std::cerr,
		(void*)vsb->getGuestAddr(), 
		(void*)vsb->getEndAddr());
	//if you want to keep going anyway, stop checking
	cross_check = NULL;
	exit(1);
}
