//#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include "syscalls.h"
#include <errno.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <asm/prctl.h>

#include "guest.h"
#include "guestcpustate.h"
#include "Sugar.h"
#include "amd64cpustate.h"

Syscalls::Syscalls(Guest* g)
: guest(g)
, sc_seen_c(0)
, exited(false)
, cpu_state(g->getCPUState())
, mappings(g->getMem())
, binary(g->getBinaryPath())
, log_syscalls(getenv("VEXLLVM_SYSCALLS") ? true : false)
{}

Syscalls::~Syscalls() {}

uint64_t Syscalls::apply(void)
{
	SyscallParams	sp(guest->getSyscallParams());
	return apply(sp);
}

/* pass through */
uint64_t Syscalls::apply(SyscallParams& args)
{
	GuestMem::Mapping	m;
	bool			fakedSyscall;
	uint64_t		sys_nr;
	unsigned long		sc_ret;

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
		sc_seen_c++;
		if (sc_seen_c >= MAX_SC_TRACE)
			sc_trace.pop_front();
		sc_trace.push_back(args);
	}

	/* this will hold any memory mapping state across the call */

	fakedSyscall = interceptSyscall(args, m, sc_ret);
	if (!fakedSyscall) {
		switch(guest->getArch()) {
		case Arch::X86_64:
			sc_ret = translateAMD64Syscall(args, m);
			break;
		case Arch::ARM:
			sc_ret = translateARMSyscall(args, m);
			break;
		case Arch::I386:
			sc_ret = translateI386Syscall(args, m);
			break;
		default:
			assert(!"unknown arch type for syscall");
		}
	}

	if (log_syscalls) {
		std::cerr << "syscall(" << sys_nr << ", "
			<< (void*)args.getArg(0) << ", "
			<< (void*)args.getArg(1) << ", "
			<< (void*)args.getArg(2) << ", "
			<< (void*)args.getArg(3) << ", "
			<< (void*)args.getArg(4) << ", ...) => "
			<< (void*)sc_ret << std::endl;
	}

	if (fakedSyscall) return sc_ret;

	//the low level sycall interface actually returns the error code
	//so we have to extract it from errno
	if(sc_ret >= 0xfffffffffffff001ULL) {
		return -errno;
	}

	/* track dynamic memory mappings */
	switch (sys_nr) {
	case SYS_mmap:
		m.offset = (void*)sc_ret;
	case SYS_mprotect:
		mappings->recordMapping(m);
		break;
	case SYS_munmap:
		mappings->removeMapping(m);
		break;
	}

	return sc_ret;
}

bool Syscalls::interceptSyscall(
	SyscallParams&		args,
	GuestMem::Mapping&	m,
	unsigned long&		sc_ret)
{
	sc_ret = 0;
	switch (args.getSyscall()) {
	case SYS_exit_group:
		exited = true;
		sc_ret = args.getArg(0);
		return true;
	case SYS_close:
		/* do not close stdin, stdout, stderr! */
		if (args.getArg(0) < 3) {
			sc_ret = 0;
			return true;
		}
		return false;
	case SYS_arch_prctl:
		switch(guest->getArch()) {
		case Arch::X86_64:
			if(AMD64_arch_prctl(args, m, sc_ret))
				return true;
			break;
		default:
			break;
		}
	case SYS_brk:
		if (!mappings->brk()) {
			/* don't let the app pull the rug */
			sc_ret = -ENOMEM;
			return true;
		}

		if (args.getArg(0) == 0) {
			/* if you're just asking, i can tell you */
			sc_ret = (unsigned long)mappings->brk();
		} else {
			if (mappings->sbrk((void*)args.getArg(0)))
				sc_ret = (uintptr_t)mappings->brk();
			else
				sc_ret = -ENOMEM;
		}

		return true;
	case SYS_rt_sigaction:
		/* for now totally lie so that code doesn't get 
		   executed without us.  we'd rather crash! */
		sc_ret = 0;
		return true;
	case SYS_mmap:
	case SYS_mprotect:
		m = GuestMem::Mapping(
			(void*)args.getArg(0),	/* offset */
			args.getArg(1),		/* length */
			args.getArg(2));

		/* mask out write permission so we can play with JITs */
		if (m.req_prot & PROT_EXEC) {
			m.cur_prot &= ~PROT_WRITE;
			args.setArg(2, m.cur_prot);
		}
		return false;

	case SYS_munmap:
		m.offset = (void*)args.getArg(0);
		m.length = args.getArg(1);
		return false;

	case SYS_readlink:
		std::string path = (char*)args.getArg(0);
		if(path != "/proc/self/exe") break;

		path = binary;
		char* buf = (char*)args.getArg(1);
		ssize_t res, last_res = 0;
		/* repeatedly deref, because exe running does it */
		for(;;) {
			res = readlink(path.c_str(), buf, args.getArg(2));
			if(res < 0) {
				return last_res ? last_res : -errno;
			}
			std::string new_path(buf, res);
			if(path == new_path)
				break;
			path = new_path;
			last_res = res;
		}
		sc_ret = res;
		return true;
	}

	return false;
}


void Syscalls::print(std::ostream& os) const
{
	foreach(it, sc_trace.begin(), sc_trace.end()) {
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

#define SYSCALL_BODY(arch, call) SYSCALL_BODY_TRICK(arch, call, _)

#define SYSCALL_BODY_TRICK(arch, call, underbar) \
	bool Syscalls::arch##underbar##call(	\
		SyscallParams&		args,	\
		GuestMem::Mapping&	m,	\
		unsigned long&		sc_ret)

SYSCALL_BODY(AMD64, arch_prctl) {
	AMD64CPUState* cpu_state = (AMD64CPUState*)this->cpu_state;
	if(args.getArg(0) == ARCH_GET_FS) {
		sc_ret = cpu_state->getFSBase();
	} else if(args.getArg(0) == ARCH_SET_FS) {
		cpu_state->setFSBase(args.getArg(1));
		sc_ret = 0;
	} else {
		/* nothing else is supported by VEX */
		sc_ret = -EPERM;
	}
	return true;
}