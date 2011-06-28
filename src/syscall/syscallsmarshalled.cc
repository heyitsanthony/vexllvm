#include <unistd.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/times.h>
#include <stdlib.h>
#include "syscallsmarshalled.h"

SyscallPtrBuf::SyscallPtrBuf(GuestMem* mem, unsigned int in_len,
 	guest_ptr in_ptr)
: ptr(in_ptr), len(in_len)
{
	if (in_ptr) {
		data = new char[len];
		mem->memcpy(data, in_ptr, len);
	} else {
		data = NULL;
		len = 0;
	}
}
SyscallsMarshalled::SyscallsMarshalled(Guest* g)
: Syscalls(g),
  last_sc_ptrbuf(NULL)
{
}

bool SyscallsMarshalled::isSyscallMarshalled(int sys_nr) const
{
	return (sys_nr == SYS_clock_gettime ||
		sys_nr == SYS_gettimeofday ||
		sys_nr == SYS_rt_sigaction ||
		sys_nr == SYS_times);
}

#include <stdio.h>

uint64_t SyscallsMarshalled::apply(SyscallParams& args)
{
	uint64_t		sys_nr;
	uint64_t		ret;

	ret = Syscalls::apply(args);
	sys_nr = args.getSyscall();

	/* store write-out data from syscall */
	assert (last_sc_ptrbuf == NULL && "Last PtrBuf not taken. Bad Sync");
	switch (sys_nr) {
	case SYS_clock_gettime:
		last_sc_ptrbuf = new SyscallPtrBuf(
			mappings,
			sizeof(struct timespec),
			guest_ptr(args.getArg(1)));
		break;
	case SYS_gettimeofday:
		last_sc_ptrbuf = new SyscallPtrBuf(
			mappings,
			sizeof(struct timeval),
			guest_ptr(args.getArg(0)));
		break;
	case SYS_rt_sigaction:
		last_sc_ptrbuf = new SyscallPtrBuf(
			mappings,
			sizeof(struct sigaction),
			guest_ptr(args.getArg(2)));
		break;
	case SYS_times:
		last_sc_ptrbuf = new SyscallPtrBuf(
			mappings,
			sizeof(struct tms),
			guest_ptr(args.getArg(0)));
		break;
	}

	return ret;
}
