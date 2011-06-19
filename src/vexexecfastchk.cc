#include <stdio.h>

#include "guestcpustate.h"
#include "vexexecfastchk.h"
#include "vexsb.h"
#include "memlog.h"
#include "ptimgchk.h"

VexExecFastChk::VexExecFastChk(PTImgChk* gs)
: VexExecChk(gs),
  pt_sc_req_c(0),
  pt_sc_done_c(0)
{
}

uint64_t VexExecFastChk::doVexSB(VexSB* vsb)
{
	MemLog	*ml;
	uint64_t new_ip;

	cross_check->breakpointSysCalls(
		(void*)vsb->getGuestAddr(),
		(void*)vsb->getEndAddr());
	if ((ml = cross_check->getMemLog())) {
		ml->clear();
		return VexExec::doVexSBAux(vsb, ml);
	} else {
		new_ip = VexExec::doVexSB(vsb);
	}
	gs->getCPUState()->setPC((void*)new_ip);

	return new_ip;
}

void VexExecFastChk::doSysCall(VexSB* vsb)
{
	VexGuestAMD64State*	state;
	bool			matched;
	void			*bp_addr;

	state = (VexGuestAMD64State*)gs->getCPUState()->getStateData();
	/* shadow process has seen k syscall opcodes (and executed k); 
	 * vexllvm process has seen k+1 syscall opcodes (and executed k)
	 */

	/* scoot shadow process up to k+1 syscall opcodes, executed k */
	bp_addr = cross_check->stepToBreakpoint();
	pt_sc_req_c++;

	/* vexllvm state expects to be past syscall, we are currently 
	 * on it. Temporarily move vexllvm back to be on syscall. */
	state->guest_RIP -= 2;
	/* In addition, vexllvm has set its RCX to the next RIP. 
	 * VexExecChk doesn't care about this since it only checks after 
	 * the syscall is complete. */
	user_regs_struct	regs;
	cross_check->getRegs(regs);
	regs.rcx = state->guest_RCX;
	cross_check->setRegs(regs);
	cross_check->resetBreakpoint(bp_addr);

	/* OK to check for match now-- fixups done */
	matched = cross_check->isMatch(*state);
	if (!matched) {
		/* instead of dying, we want to be smarter here by
		 * restarting the process, fastforwarding to last OK
		 * syscall, then finally single step up to bad syscall. */
		fprintf(stderr, "MISMATCH: BEGINNING OF SYSCALL\n");
		dumpSubservient(vsb);
		assert (0 == 1 && "FIXME LATER");
	}

	/* restore vexllvm IP to *after* syscall */
	state->guest_RIP += 2;
	
	/* recall: shadow process <= vexllvm. hence, we must call the 
	 * syscall for vexllvm prior to the syscall for the shadow process.
	 * this lets us to fixups to match vexllvm's calls. */
	VexExec::doSysCall(vsb);

	stepSysCall(vsb);
	pt_sc_done_c++;	

	/* now both should be equal and at the instruction immediately following
	 * the breaking syscall */
	if (!cross_check->isMatch(*state)) {
		fprintf(stderr, "MISMATCH: END OF SYSCALL.\n");
		dumpSubservient(vsb);
	}

	cross_check->setBreakpoint(bp_addr);
}
