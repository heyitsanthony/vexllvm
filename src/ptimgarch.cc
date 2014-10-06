#include <stdio.h>
#include <stdlib.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include "ptimgarch.h"
#include "guestcpustate.h"

PTImgArch::PTImgArch(GuestPTImg* in_gs, int in_pid)
: gs(in_gs)
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

void PTImgArch::pushBadProgress(void)
{
	guest_ptr	cur_pc(gs->getCPUState()->getPC());
	gs->getCPUState()->setPC(guest_ptr(0xbadbadbadbad));
	pushRegisters();
	gs->getCPUState()->setPC(cur_pc);
}

void PTImgArch::waitForSingleStep(void)
{
	int	err;

	steps++;
	err = ptrace(PTRACE_SINGLESTEP, child_pid, NULL, NULL);
	if(err < 0) {
		perror("PTImgChk::doStep ptrace single step");
		exit(1);
	}
	wait(&wss_status);

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

	if (log_gauge_overflow && (steps % log_gauge_overflow) == 0) {
		char	c = "/-\\|/-\\|"[(steps / log_gauge_overflow)%8];
		fprintf(stderr,
			//"STEPS %09"PRIu64" %c %09"PRIu64" BLOCKS\r",
			"STEPS %09" PRIu64 " %c %09" PRIu64 " BLOCKS\n",
			steps, c, blocks);
	}
}

void PTImgArch::copyIn(guest_ptr dst, const void* src, unsigned int bytes) const
{
	const char*	in_addr;
	guest_ptr	out_addr, end_addr;

	assert ((bytes % sizeof(long)) == 0);

	in_addr = (const char*)src;
	out_addr = dst;
	end_addr = out_addr + bytes;

	while (out_addr < end_addr) {
		long data = *(const long*)in_addr;
		int err = ptrace(PTRACE_POKEDATA, child_pid, out_addr.o, data);
		assert(err == 0);
		in_addr += sizeof(long);
		out_addr.o += sizeof(long);
	}
}

bool PTImgArch::isSigSegv(void) const { return WSTOPSIG(wss_status) == SIGSEGV; }
