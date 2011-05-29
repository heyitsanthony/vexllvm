#include <unistd.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <string.h>
#include <sys/signal.h>

#include "syscallsmarshalled.h"

SyscallPtrBuf::SyscallPtrBuf(unsigned int in_len, void* in_ptr)
: ptr(in_ptr), len(in_len)
{
	if (in_ptr) {
		data = new char[len];
		memcpy(data, in_ptr, len);
	} else {
		data = NULL;
		len = 0;
	}
}

bool SyscallsMarshalled::isSyscallMarshalled(int sys_nr) const
{
	return (sys_nr == SYS_clock_gettime || 
		sys_nr == SYS_gettimeofday || 
		sys_nr == SYS_rt_sigaction);
}

#include <stdio.h>

uint64_t SyscallsMarshalled::apply(const SyscallParams& args)
{
	uint64_t		sys_nr;
	uint64_t		ret;

	ret = Syscalls::apply(args);

	/* store write-out data from syscall */
	assert (last_sc_ptrbuf == NULL && "Last PtrBuf not taken. Bad Sync");
	sys_nr = args.getSyscall();
	switch (sys_nr) {
	case SYS_clock_gettime:
		last_sc_ptrbuf = new SyscallPtrBuf(
			sizeof(struct timespec),
			(void*)args.getArg(1));
		break;
	case SYS_gettimeofday:
		last_sc_ptrbuf = new SyscallPtrBuf(
			sizeof(struct timeval),
			(void*)args.getArg(0));
		break;
	case SYS_rt_sigaction:
		last_sc_ptrbuf = new SyscallPtrBuf(
			sizeof(struct sigaction), 
			(void*)args.getArg(2));
		break;
	}

	return ret;
}
