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

#include "guestcpustate.h"
#include "Sugar.h"

Syscalls::Syscalls(GuestCPUState& in_cpu_state, VexMem& in_mappings, 
	const char* in_binary)
: sc_seen_c(0)
, exited(false)
, cpu_state(in_cpu_state)
, mappings(in_mappings)
, binary(in_binary)
, log_syscalls(getenv("VEXLLVM_SYSCALLS") ? true : false)
{}

Syscalls::~Syscalls() {}

/* pass through */
uint64_t Syscalls::apply(SyscallParams& args)
{
	uint64_t	sys_nr;
	unsigned long	sc_ret;

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
	VexMem::Mapping m;
	
	bool handled = false;

	switch (sys_nr) {
	case SYS_exit_group:
		exited = true;
		sc_ret = args.getArg(0);
		handled = true;
		break;
	case SYS_close:
		/* do not close stdin, stdout, stderr! */
		if (args.getArg(0) < 3) {
			sc_ret = 0;
			handled = true;
		}
		break;
	case SYS_arch_prctl:
		/* arrrrg... this is a little too x86 specific */
		if(args.getArg(0) == ARCH_GET_FS) {
			sc_ret = cpu_state.getFSBase();
		} else if(args.getArg(0) == ARCH_SET_FS) {
			cpu_state.setFSBase(args.getArg(1));
			sc_ret = 0;
		} else {
			/* nothing else is supported by VEX */
			sc_ret = -EPERM;
		}
		handled = true;
		break;
	case SYS_brk:
		if(mappings.brk()) {
			if(args.getArg(0) == 0) {
				/* if your just asking, i can tell you */
				sc_ret = (unsigned long)mappings.brk();
			} else {
				bool r = mappings.sbrk((void*)args.getArg(0));
				if(r) {
					sc_ret = (uintptr_t)mappings.brk();
				} else {
					sc_ret = -ENOMEM;
				}
			}
		} else {
			/* don't let the app pull the rug */
			sc_ret = -ENOMEM;
		}
		handled = true;
		break;
	case SYS_mmap:
	case SYS_mprotect:
		m.offset = (void*)args.getArg(0);
		m.length = args.getArg(1);
		m.req_prot = args.getArg(2);
		m.cur_prot = m.req_prot;
		/* mask out write permission so we can play with JITs */
		if(m.req_prot & PROT_EXEC) {
			m.cur_prot &= ~PROT_WRITE;
			args.setArg(2, m.cur_prot);
		}
		break;
	case SYS_munmap:
		m.offset = (void*)args.getArg(0);
		m.length = args.getArg(1);
		break;
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
		handled = true;
		break;
	}

	if(!handled)
		sc_ret = syscall(
			sys_nr,
			args.getArg(0),
			args.getArg(1),
			args.getArg(2),
			args.getArg(3),
			args.getArg(4),
			args.getArg(5));

	if(log_syscalls) {
		std::cerr << "syscall(" << sys_nr << ", "
			<< (void*)args.getArg(0) << ", "
			<< (void*)args.getArg(1) << ", "
			<< (void*)args.getArg(2) << ", "
			<< (void*)args.getArg(3) << ", "
			<< (void*)args.getArg(4) << ", ...) => "
			<< (void*)sc_ret << std::endl;
	}

	if(handled)
		return sc_ret;
		
	//the low level sycall interface actually returns the error code
	//so we have to extract it from errno
	if(sc_ret >= 0xfffffffffffff001ULL) {
		return -errno;
	}

	/* track dynamic memory mappings */
	switch (sys_nr) {
	case SYS_mmap:
		m.offset = (void*)sc_ret;
		mappings.recordMapping(m);
		break;
	case SYS_mprotect:
		mappings.recordMapping(m);
		break;
	case SYS_munmap:
		mappings.removeMapping(m);
		break;
	}

	return sc_ret;
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
