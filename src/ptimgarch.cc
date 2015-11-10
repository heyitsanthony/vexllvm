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
, bp_steps(0)
, blocks(0)
, hit_syscall(false)
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

guest_ptr PTImgArch::stepToBreakpoint(void)
{
	int	err, status;

	bp_steps++;
	pt_cpu->revokeRegs();
	err = ptrace(PTRACE_CONT, child_pid, NULL, NULL);
	if(err < 0) {
		perror("PTImgArch::doStep ptrace single step");
		exit(1);
	}
	wait(&status);

	if (!(WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP)) {
		fprintf(stderr,
			"OOPS. status: stopped=%d sig=%d status=%p\n",
			WIFSTOPPED(status), WSTOPSIG(status),
			(void*)(long)status);
		// stackTraceShadow(std::cerr, guest_ptr(0),  guest_ptr(0));
		assert (0 == 1 && "bad wait from breakpoint");
	}

	/* ptrace executes trap, so child process's IP needs to be fixed */
	return pt_cpu->undoBreakpoint();
}

void PTImgArch::stepThroughBounds(guest_ptr start, guest_ptr end)
{
	guest_ptr	new_ip;

	/* TODO: timeout? */
	do {
		new_ip = continueForwardWithBounds(start, end);
		if (hit_syscall)
			break;
	} while (new_ip >= start && new_ip < end);
}

guest_ptr PTImgArch::continueForwardWithBounds(guest_ptr start, guest_ptr end)
{
	if (log_steps) {
		std::cerr << "RANGE: "
			<< (void*)start.o << "-" << (void*)end.o
			<< std::endl;
	}

	hit_syscall = false;
	while (doStep(start, end, hit_syscall)) {
		guest_ptr	pc(pt_cpu->getPC());
		/* so we trap on backjumps */
		if (pc > start)
			start = pc;
	}

	incBlocks();
	return pt_cpu->getPC();
}
