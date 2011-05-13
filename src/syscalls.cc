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
	call_trace.push_back(args.getSyscall());
	return syscall(
		args.getSyscall(), 
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
