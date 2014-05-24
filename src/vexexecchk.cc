#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <sys/syscall.h>


#include "vexxlate.h"
#include "vexsb.h"

#include "guestcpustate.h"
#include "memlog.h"
#include "ptimgchk.h"
#include "syscall/syscallsmarshalled.h"
#include "vexexecchk.h"

VexExecChk::VexExecChk(PTImgChk* gs, VexXlate* vx)
: VexExec(gs, vx)
, hit_syscall(false)
, is_deferred(false)
{
	cross_check = (getGuest()) ? gs : NULL;
	if (!cross_check) return;

	if (sc != NULL) delete sc;
	sc = new SyscallsMarshalled(gs);
}

/* ensures that shadow process's state <= llvm process's state */
guest_ptr VexExecChk::doVexSB(VexSB* vsb)
{
	MemLog		*ml;
	bool		new_ip_in_bounds;
	guest_ptr	new_ip;

	/* if we're not deferred we know we're synced up already.
	 * this fact to be paranoid (make sure the states are ==)*/
	if (!is_deferred) {
		if (!cross_check->isMatch()) {
			fprintf(stderr, "MISMATCH PRIOR TO doVexSB. HOW??\n");
			dumpShadow(vsb);
		}
	}

	if ((ml = cross_check->getMemLog())) {
		ml->clear();
		new_ip = VexExec::doVexSBAux(vsb, ml);
	} else
		new_ip = VexExec::doVexSB(vsb);

	gs->getCPUState()->setPC(new_ip);

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
			vsb->getEndAddr());

		is_deferred = false;
		if (vsb->isSyscall()) {
			hit_syscall = true;
			goto done;
		}

		goto states_should_be_equal;
	} else if (is_deferred && !new_ip_in_bounds) {
		/* can step out of the block now we call this a "resume" */
		cross_check->stepThroughBounds(
			deferred_bound_start, deferred_bound_end);
		is_deferred = false;
		deferred_bound_start = guest_ptr(0);
		deferred_bound_end = guest_ptr(0);
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
	new_ip = gs->getCPUState()->getPC();
done:
	return new_ip;
}

/* check, fix, die */
void VexExecChk::verifyBlockRun(VexSB* vsb)
{
	bool fixed;

	/* OK. */
	if (cross_check->isMatch())
		 return;

#ifndef BROKEN_OSDI
	/* bad address? */
	if (	gs->getMem()->isMapped(gs->getCPUState()->getPC()) == false &&
		cross_check->getPTArch()->isSigSegv() == true)
	{
		fprintf(stderr,
			"[VexExecChk] Jump to invalid address %p\n",
			(void*)gs->getCPUState()->getPC().o);
		exit(0);
	}
#endif

	fixed = cross_check->fixup(vsb->getInstExtents());
	if (fixed) {
		/* if registers were slurped into the guest
		 * then the 'next_addr' is invalid. The PC in
		 * the state is accurate, so use it instead. */
		next_addr = gs->getCPUState()->getPC();
		return;
	}

	fprintf(stderr, "MISMATCH: END OF BLOCK. FIND NEW EMU BUG.\n");
	dumpShadow(vsb);
}

void VexExecChk::stepSysCall(VexSB* vsb)
{
	cross_check->stepSysCall(static_cast<SyscallsMarshalled*>(sc));
	gs->getCPUState()->resetSyscall();
}

void VexExecChk::doSysCallCore(VexSB* vsb)
{
	/* recall: shadow process <= vexllvm. hence, we must call the 
	 * syscall for vexllvm prior to the syscall for the shadow process.
	 * this lets us to fixups to match vexllvm's calls. */
	SyscallParams	sp(gs->getSyscallParams());
	int		sys_nr;

	sys_nr = sc->translateSyscall(sp.getSyscall());
	if (sys_nr == SYS_brk) {
		/* XXX audit this code. I don't trust it-- AJR */

		/* but for brk, we really need to copy the state over
		   from the child process or we would need to fully
		   emulate brk there by creating memory mappings in the
		   other process... blek to that... no nice api */

		stepSysCall(vsb);
		sp.setArg(0, cross_check->getSysCallResult());
		VexExec::doSysCall(vsb, sp);
		return;
	}

	VexExec::doSysCall(vsb, sp);
	stepSysCall(vsb);
}

void VexExecChk::doSysCall(VexSB* vsb)
{
	assert (hit_syscall && !is_deferred);

	doSysCallCore(vsb);

	/* now both should be equal and at the instruction following
	 * the breaking syscall */
	if (!cross_check->isMatch()) {
		fprintf(stderr, "MISMATCH: END OF SYSCALL. SYSEMU BUG.\n");
		dumpShadow(vsb);
	}

	hit_syscall = false;
}

void VexExecChk::dumpShadow(VexSB* vsb)
{
	if (vsb != NULL) {
		std::cerr << 
			"found divergence running block @ " <<
			(void*)vsb->getGuestAddr().o << std::endl;

		std::cerr << 
			"original block end was @ " <<
			(void*)vsb->getEndAddr().o << std::endl;
	}

	cross_check->printTraceStats(std::cerr);
	std::cerr << "PTRACE state" << std::endl;
	cross_check->printShadow(std::cerr);

	std::cerr << "VEXLLVM state" << std::endl;
	gs->print(std::cerr);

	if (vsb != NULL) {
		std::cerr << "VEX IR" << std::endl;
		vsb->print(std::cerr);
	}

	std::cerr << "PTRACE stack" << std::endl;
	cross_check->stackTraceShadow(
		std::cerr,
		vsb->getGuestAddr(), 
		vsb->getEndAddr());
		
	//if you want to keep going anyway, stop checking
	cross_check = NULL;
	exit(1);
}
