#include <stdio.h>
#include <stdlib.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include "ptimgarch.h"
#include "ptcpustate.h"

PTImgArch::PTImgArch(GuestPTImg* in_gs, int in_pid)
: gs(in_gs)
, pt_cpu(nullptr)
, child_pid(in_pid)
, steps(0)
, blocks(0)
{
	const char* step_gauge;

	log_steps = (getenv("VEXLLVM_LOG_STEPS")) ? true : false;

	step_gauge = getenv("VEXLLVM_STEP_GAUGE");
	if (step_gauge == NULL) {
		log_gauge_overflow = 0;
	} else {
		log_gauge_overflow = atoi(step_gauge);
		fprintf(stderr,
			"STEPS BETWEEN UPDATE: %d.\n",
			log_gauge_overflow);
	}
}

PTImgArch::~PTImgArch() {}

bool PTImgArch::breakpointSysCalls(
	const guest_ptr ip_begin,
	const guest_ptr ip_end)
{
	guest_ptr	pc = ip_begin;
	bool		set_bp = false;

	while (pc != ip_end) {
		if (pt_cpu->isSyscallOp(pc, getInsOp(pc))) {
			gs->setBreakpoint(pc);
			set_bp = true;
		}
		pc.o++;
	}

	return set_bp;
}

long PTImgArch::getInsOp(void) const { return getInsOp(pt_cpu->getPC().o); }

long PTImgArch::getInsOp(long pc) const
{
	if ((uintptr_t)pc== chk_addr.o)
		return chk_opcode;

	chk_addr = guest_ptr(pc);
// SLOW WAY:
// Don't need to do this so long as we have the data at chk_addr in the guest
// process also mapped into the parent process at chk_addr.
//	chk_opcode = ptrace(PTRACE_PEEKTEXT, child_pid, regs.rip, NULL);

// FAST WAY: read it off like a boss
	chk_opcode = *((const long*)gs->getMem()->getHostPtr(chk_addr));
	return chk_opcode;
}

void PTImgArch::pushBadProgress(void)
{
	guest_ptr	cur_pc(gs->getCPUState()->getPC());
	gs->getCPUState()->setPC(guest_ptr(0xbadbadbadbad));
	pushRegisters();
	gs->getCPUState()->setPC(cur_pc);
}

uint64_t PTImgArch::dispatchSysCall(const SyscallParams& sp)
{
	uint64_t ret = pt_cpu->dispatchSysCall(sp, wss_status);
	steps++;
	checkWSS();
	return ret;
}

void PTImgArch::checkWSS(void)
{
	//TODO: real signal handling needed, but the main process
	//doesn't really have that yet...
	// 1407
	if (WIFSTOPPED(wss_status)) {
		switch (WSTOPSIG(wss_status)) {
		case SIGTRAP:
			/* trap is expected for single-stepping */
			break;
		case SIGFPE:
			fprintf(stderr, "[PTImgArch] FPE!\n");
			pushBadProgress();
			break;

		case SIGSEGV:
			fprintf(stderr, "[PTImgArch] SIGSEGV!\n");
			pushBadProgress();
			break;

		case SIGILL: {
			fprintf(stderr,
				"[PTImgArch] Illegal instruction at %p!!!\n",
				(void*)gs->getCPUState()->getPC().o);
			pushBadProgress();
			break;
		}
		default:
			fprintf(stderr,
				"[PTImgArch] ptrace bad single step status=%d\n",
				WSTOPSIG(wss_status));
			pushBadProgress();
			break;
		}
	} else {
		fprintf(stderr, "STATUS: %lx\n", (unsigned long)wss_status);
		perror("Unknown status in waitForSingleStep");
		abort();
	}
}

void PTImgArch::waitForSingleStep(void)
{
	steps++;
	wss_status = pt_cpu->waitForSingleStep(); 
	checkWSS();
	pt_cpu->revokeRegs();

	if (log_gauge_overflow && (steps % log_gauge_overflow) == 0) {
		char	c = "/-\\|/-\\|"[(steps / log_gauge_overflow)%8];
		fprintf(stderr,
			//"STEPS %09"PRIu64" %c %09"PRIu64" BLOCKS\r",
			"STEPS %09" PRIu64 " %c %09" PRIu64 " BLOCKS\n",
			steps, c, blocks);
	}
}

bool PTImgArch::isSigSegv(void) const { return WSTOPSIG(wss_status) == SIGSEGV; }
