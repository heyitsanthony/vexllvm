#include <stdio.h>

#include "guestcpustate.h"
#include "vexexecfastchk.h"
#include "vexsb.h"
#include "memlog.h"
#include "ptimgchk.h"

VexExecFastChk::VexExecFastChk(PTImgChk* gs, std::shared_ptr<VexXlate> vx)
: VexExecChk(gs, vx),
  pt_sc_req_c(0),
  pt_sc_done_c(0)
{
}

guest_ptr VexExecFastChk::doVexSB(VexSB* vsb)
{
	MemLog	*ml;
	guest_ptr new_ip;

	cross_check->breakpointSysCalls(
		vsb->getGuestAddr(),
		vsb->getEndAddr());
	if ((ml = cross_check->getMemLog())) {
		ml->clear();
		return VexExec::doVexSBAux(vsb, ml);
	} else {
		new_ip = VexExec::doVexSB(vsb);
	}

	gs->getCPUState()->setPC(new_ip);

	return new_ip;
}

#if __amd64__
extern "C" {
#include <valgrind/libvex_guest_amd64.h>
}
#include "cpu/ptimgamd64.h"
#endif

void VexExecFastChk::doSysCall(VexSB* vsb)
{
#if __amd64__
	VexGuestAMD64State*	state;
	PTImgAMD64		*pt_arch;
	bool			matched;
	guest_ptr		bp_addr;

	pt_arch = static_cast<PTImgAMD64*>(cross_check->getPTArch());
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

	pt_arch->getRegs(regs);
	regs.rcx = state->guest_RCX;
	pt_arch->setRegs(regs);
	cross_check->resetBreakpoint(bp_addr);

	/* OK to check for match now-- fixups done */
	matched = cross_check->isMatch();
	if (!matched) {
		/* instead of dying, we want to be smarter here by
		 * restarting the process, fastforwarding to last OK
		 * syscall, then finally single step up to bad syscall. */
		fprintf(stderr, "MISMATCH: BEGINNING OF SYSCALL\n");
		dumpShadow(vsb);
		assert (0 == 1 && "FIXME LATER");
	}

	/* restore vexllvm IP to *after* syscall */
	state->guest_RIP += 2;

	doSysCallCore(vsb);

	pt_sc_done_c++;

	/* now both should be equal and at the instruction immediately following
	 * the breaking syscall */
	if (!cross_check->isMatch()) {
		fprintf(stderr, "MISMATCH: END OF SYSCALL.\n");
		dumpShadow(vsb);
	}

	cross_check->setBreakpoint(bp_addr);
#else
	assert (0 == 1 && "ARCH STUB");
#endif
}
