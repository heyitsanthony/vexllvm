#include "syscalls.h"
#include "guest.h"
#include <sys/syscall.h>
#include "amd64cpustate.h"
#include <sys/errno.h>
#include <sys/prctl.h>
#include <asm/prctl.h>

int Syscalls::translateAMD64Syscall(int sys_nr) {
	assert(guest->getArch() == Arch::getHostArch());
	return sys_nr;
}

uintptr_t Syscalls::applyAMD64Syscall(
	SyscallParams& args,
	GuestMem::Mapping& m)
{
	uintptr_t sc_ret = ~0ULL;

	/* special syscalls that we handle per arch, these
	   generally supersede any pass through or translated 
	   behaviors, but they can just alter the args and let
	   the other mechanisms finish the job */
	switch (args.getSyscall()) {
	case SYS_arch_prctl:
		if(AMD64_arch_prctl(args, m, sc_ret))
			return sc_ret;
		break;
	default:
		break;
	}
		
	/* if the host and guest are identical, then just pass through */
	if(guest->getArch() == Arch::getHostArch()) {
		return passthroughSyscall(args, m);
	}
	
	std::cerr << "syscall(" << args.getSyscall() << ", "
		<< (void*)args.getArg(0) << ", "
		<< (void*)args.getArg(1) << ", "
		<< (void*)args.getArg(2) << ", "
		<< (void*)args.getArg(3) << ", "
		<< (void*)args.getArg(4) << ", ...) => ???"
		<< std::endl;
	assert(!"supporting translation between syscalls for these cpus");
}

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