//#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "syscalls.h"

Syscalls::Syscalls() {}
Syscalls::~Syscalls() {}

/* pass through */
uint64_t Syscalls::apply(const SyscallParams& args)
{
	return syscall(
		args.getSyscall(), 
		args.getArg(0),
		args.getArg(1),
		args.getArg(2));
}