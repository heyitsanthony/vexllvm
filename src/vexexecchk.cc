#include <iostream>
#include <stdlib.h>
#include <stdio.h>


#include "vexxlate.h"
#include "vexsb.h"

#include "guestcpustate.h"
#include "ptimgchk.h"
#include "vexexecchk.h"

void VexExecChk::compareWithSubservient(VexSB* vsb)
{
	const VexGuestAMD64State*	state;
	bool matched;
	
	state = (const VexGuestAMD64State*)gs->getCPUState()->getStateData();
	matched = cross_check->continueWithBounds(
		vsb->getGuestAddr(),
		vsb->getEndAddr(),
		*state);
	if (matched) return;

	dumpSubservient(vsb);
}

uint64_t VexExecChk::doVexSB(VexSB* vsb)
{
	uint64_t	new_ip;

	new_ip = VexExec::doVexSB(vsb);
	if(new_ip < vsb->getGuestAddr() || new_ip >= vsb->getEndAddr()) {
		compareWithSubservient(vsb);
	}

	return new_ip;
}

VexExec* VexExecChk::create(PTImgChk* gs)
{
	VexExecChk	*ve_chk;

	setupStatics(gs);
	ve_chk = new VexExecChk(gs);
	if (ve_chk->cross_check == NULL) {
		delete ve_chk;
		return NULL;
	}

	return ve_chk;
}

// must do the check after the sycall is executed
void VexExecChk::handlePostSyscall(VexSB* vsb, uint64_t new_jmpaddr)
{
	VexGuestAMD64State*	state;
	bool			matched;
	
	state = (VexGuestAMD64State*)gs->getCPUState()->getStateData();
	matched = cross_check->continueWithBounds(
		vsb->getGuestAddr(), vsb->getEndAddr(), *state);
	
	if (matched) return;

	if (	((uint64_t)new_jmpaddr < vsb->getGuestAddr() || 
		(uint64_t)new_jmpaddr >= vsb->getEndAddr()))
	{
		dumpSubservient(vsb);
	}
}
	
void VexExecChk::doSysCall(void)
{
	VexGuestAMD64State* state;

	VexExec::doSysCall();

	state = (VexGuestAMD64State*)gs->getCPUState()->getStateData();
	state->guest_RCX = 0;
	state->guest_R11 = 0;
}

void VexExecChk::dumpSubservient(VexSB* vsb)
{
	const VexGuestAMD64State* state;
	
	state = (const VexGuestAMD64State*)gs->getCPUState()->getStateData();
	std::cerr << "found divergence running block @ " << (void*)vsb->getGuestAddr() << std::endl;
	std::cerr << "original block end was @ " << (void*)vsb->getEndAddr() << std::endl;
	cross_check->printTraceStats(std::cerr);
	std::cerr << "PTRACE state" << std::endl;
	cross_check->printSubservient(std::cerr, *state);
	std::cerr << "VEXLLVM state" << std::endl;
	gs->print(std::cerr);
	std::cerr << "VEX IR" << std::endl;
	vsb->print(std::cerr);
	std::cerr << "PTRACE stack" << std::endl;
	cross_check->stackTraceSubservient(std::cerr);
	//if you want to keep going anyway, stop checking
	cross_check = NULL;
	exit(1);
}
