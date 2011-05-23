//#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "syscalls.h"

#include "Sugar.h"

Syscalls::Syscalls() : exited(false) {}
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
		call_trace.push_back(args);
	}

	if (sys_nr == SYS_exit_group) {
		exited = true;
		return args.getArg(0);
	} else if (sys_nr == SYS_close) {
		/* do not close stdin, stdout, stderr! */
		if (args.getArg(0) < 3)
			return 0;
	} else if (sys_nr == SYS_brk) {
		/* don't let the app pull the rug from under us */
		return -1;
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
		SyscallParams	sp = *it;
		os << "Syscall: NR=" << sp.getSyscall();
		os << " {"
			<< (void*)sp.getArg(0) << ", "
			<< (void*)sp.getArg(1) << ", "
			<< (void*)sp.getArg(2) << ", "
			<< (void*)sp.getArg(3) << ", "
			<< (void*)sp.getArg(4) << ", "
			<< (void*)sp.getArg(5) << "}" << std::endl;
	}
}
