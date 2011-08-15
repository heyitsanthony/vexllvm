//#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include "syscalls.h"
#include <errno.h>
#include <stdlib.h>

#include "guest.h"
#include "guestcpustate.h"
#include "Sugar.h"

Syscalls::Syscalls(Guest* g)
: guest(g)
, sc_seen_c(0)
, exited(false)
, cpu_state(g->getCPUState())
, mappings(g->getMem())
, binary(g->getBinaryPath())
, log_syscalls(getenv("VEXLLVM_SYSCALLS") ? true : false)
, force_translation(getenv("VEXLLVM_XLATE_SYSCALLS") ? true : false)
{
}

const std::string Syscalls::chroot(getenv("VEXLLVM_CHROOT") ?
	getenv("VEXLLVM_CHROOT") : "");
	
Syscalls::~Syscalls() {}

uint64_t Syscalls::apply(void)
{
	SyscallParams	sp(guest->getSyscallParams());
	return apply(sp);
}

/* pass through */
uint64_t Syscalls::apply(SyscallParams& args)
{
	bool			fakedSyscall;
	uint64_t		sys_nr;
	uintptr_t 		sc_ret = ~0ULL;

	/* translate the syscall number by architecture so that
	   we can have boiler plate implementations centralized
	   in this file */
	sys_nr = args.getSyscall();
	sys_nr = translateSyscall(sys_nr);

	
	/* check for syscalls which would mess with the thread or process
	   state and otherwise render us useless */
	switch (sys_nr) {
#define BAD_SYSCALL(x)	\
	case x: 	\
		fprintf(stderr, "UNHANDLED SYSCALL: %s\n", #x); \
		assert (0 == 1 && "TRICKY SYSCALL");

	BAD_SYSCALL(SYS_clone)
	BAD_SYSCALL(SYS_fork)
	// BAD_SYSCALL(SYS_exit) ok for now since clone isnt possible
	BAD_SYSCALL(SYS_execve)
	default:
		sc_seen_c++;
		if (sc_seen_c >= MAX_SC_TRACE)
			sc_trace.pop_front();
		sc_trace.push_back(args);
	}

	/* this will hold any memory mapping state across the call.
	   it will be updated by the code which applies the platform
	   dependent syscall to provide a mapping update.  this is done
	   for new mappings, changed mapping, and unmappings */
	GuestMem::Mapping	m;

	fakedSyscall = interceptSyscall(sys_nr, args, sc_ret);
	if (!fakedSyscall) {
		switch(guest->getArch()) {
		case Arch::X86_64:
			sc_ret = applyAMD64Syscall(args);
			break;
		case Arch::ARM:
			sc_ret = applyARMSyscall(args);
			break;
		case Arch::I386:
			sc_ret = applyI386Syscall(args);
			break;
		default:
			assert(!"unknown arch type for syscall");
		}
	}

	if (log_syscalls) {
		print(std::cerr, args, &sc_ret);
	}

	return sc_ret;
}

int Syscalls::translateSyscall(int sys_nr) const {
	switch(guest->getArch()) {
	case Arch::X86_64:
		return translateAMD64Syscall(sys_nr);
	case Arch::ARM:
		return translateARMSyscall(sys_nr);
	case Arch::I386:
		return translateI386Syscall(sys_nr);
	default:
		assert(!"unknown arch type for syscall");
	}	
}

std::string Syscalls::getSyscallName(int sys_nr) const {
	switch(guest->getArch()) {
	case Arch::X86_64:
		return getAMD64SyscallName(sys_nr);
	case Arch::ARM:
		return getARMSyscallName(sys_nr);
	case Arch::I386:
		return getI386SyscallName(sys_nr);
	default:
		assert(!"unknown arch type for syscall");
	}
}

/* it is disallowed to implement any syscall which requires access to
   data in a structure within this function.  this function does not
   understand the potentially different layout of the guests syscalls
   so, syscalls with non-raw pointer data must be handled in the 
   architecture specific code */
bool Syscalls::interceptSyscall(
	int sys_nr,
	SyscallParams&		args,
	unsigned long&		sc_ret)
{
	switch (sys_nr) {
	case SYS_exit_group:
		exited = true;
		sc_ret = args.getArg(0);
		return true;
	case SYS_exit:
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
	case SYS_dup2:
		/* do not close stdin, stdout, stderr! */
		if (args.getArg(1) < 3) {
			sc_ret = 0;
			return true;
		}
		return false;
	case SYS_brk:
		if (mappings->sbrk(guest_ptr(args.getArg(0))))
			sc_ret = mappings->brk();
		else
			sc_ret = -ENOMEM;
		return true;
	case SYS_rt_sigaction:
		/* for now totally lie so that code doesn't get 
		   executed without us.  we'd rather crash! */
		sc_ret = 0;
		return true;
	case SYS_mmap: {
		guest_ptr m;
		sc_ret = mappings->mmap(m, 
			guest_ptr(args.getArg(0)),
			args.getArg(1), 
			args.getArg(2), 
			args.getArg(3), 
			args.getArg(4), 
			args.getArg(5));
		if(sc_ret == 0)
			sc_ret = m;
		return true;
	}
	case SYS_mremap: {
		guest_ptr m;
		sc_ret = mappings->mremap(m, 
			guest_ptr(args.getArg(0)),
			args.getArg(1), 
			args.getArg(2), 
			args.getArg(3), 
			guest_ptr(args.getArg(4)));
		if(sc_ret == 0)
			sc_ret = m;
		return true;
	}
	case SYS_mprotect:
		sc_ret = mappings->mprotect(
			guest_ptr(args.getArg(0)),
			args.getArg(1),
			args.getArg(2));
		return true;
	case SYS_munmap:
		sc_ret = mappings->munmap(
			guest_ptr(args.getArg(0)),
			args.getArg(1));
		return true;
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

uintptr_t Syscalls::passthroughSyscall(
	SyscallParams& args)
{
	uintptr_t sc_ret = syscall(
		args.getSyscall(),
		args.getArg(0),
		args.getArg(1),
		args.getArg(2),
		args.getArg(3),
		args.getArg(4),
		args.getArg(5));
	/* the low level sycall interface actually returns the error code
	   so we have to extract it from errno if we did blind syscall 
	   pass through */
	if(sc_ret >= 0xfffffffffffff001ULL) {
		return -errno;
	}
	return sc_ret;
}

void Syscalls::print(std::ostream& os) const
{
	foreach(it, sc_trace.begin(), sc_trace.end()) {
		SyscallParams	sp = *it;
		print(os, sp, NULL);
	}
}
void Syscalls::print(std::ostream& os, const SyscallParams& sp, 
	uintptr_t* result) const
{
	os << "Syscall: " << sp.getSyscall() << " : " << getSyscallName(sp.getSyscall());
	os << " {"
		<< (void*)sp.getArg(0) << ", "
		<< (void*)sp.getArg(1) << ", "
		<< (void*)sp.getArg(2) << ", "
		<< (void*)sp.getArg(3) << ", "
		<< (void*)sp.getArg(4) << ", "
		<< (void*)sp.getArg(5) << "}";
	if(result)
		os << " => " << (void*)*result;
	os << std::endl;
}
