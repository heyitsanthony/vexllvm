//#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "syscalls.h"

#include "Sugar.h"

Syscalls::Syscalls() {}
Syscalls::~Syscalls() {}

/* pass through */
uint64_t Syscalls::apply(const SyscallParams& args)
{
	uint64_t	sys_nr;

	sys_nr = args.getSyscall();
	switch (sys_nr) {
#define BAD_SYSCALL(x)	\
	case x: 	\
		fprintf(stderr, "UNHANDLED SYSCALL: %s\n", #x); \
		assert (0 == 1 && "TRICKY SYSCALL");

	BAD_SYSCALL(SYS_clone)
	BAD_SYSCALL(SYS_fork)
	BAD_SYSCALL(SYS_exit)
	BAD_SYSCALL(SYS_execve)
	default:
		call_trace.push_back(sys_nr);
	}

	return syscall(
		sys_nr, 
		args.getArg(0),
		args.getArg(1),
		args.getArg(2),
		args.getArg(3),
		args.getArg(4),
		args.getArg(5));
}

void Syscalls::print(std::ostream& os) const
{
	foreach(it, call_trace.begin(), call_trace.end()) {
		os << "Syscall: Num=" << (*it) << std::endl;
	}
}
