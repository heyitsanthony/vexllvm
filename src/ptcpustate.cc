#include <sys/ptrace.h>
#include <sys/wait.h>

#include "ptcpustate.h"

int PTCPUState::waitForSingleStep(void)
{
	int err = ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
	int wss_status = 0;

	if(err < 0) {
		perror("PTImgChk::doStep ptrace single step");
		exit(1);
	}

	wait(&wss_status);
	return wss_status;
}

void PTCPUState::resetBreakpoint(guest_ptr addr, long v)
{
	int err = ptrace(PTRACE_POKETEXT, pid, addr.o, v);
	assert (err != -1 && "Failed to reset breakpoint");
}

void PTCPUState::copyIn(guest_ptr dst, const void* src, unsigned int bytes) const
{
	const char*	in_addr;
	guest_ptr	out_addr, end_addr;

	assert ((bytes % sizeof(long)) == 0);

	in_addr = (const char*)src;
	out_addr = dst;
	end_addr = out_addr + bytes;

	while (out_addr < end_addr) {
		long data = *(const long*)in_addr;
		int err = ptrace(PTRACE_POKEDATA, pid, out_addr.o, data);
		assert(err == 0);
		in_addr += sizeof(long);
		out_addr.o += sizeof(long);
	}
}

