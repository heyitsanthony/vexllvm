#include "syscalls.h"
#include "guest.h"

uintptr_t Syscalls::translateARMSyscall(
	SyscallParams& args,
	GuestMem::Mapping& m)
{
	if(guest->getArch() == Guest::getHostArch()) {
		return syscall(
			args.getSyscall(),
			args.getArg(0),
			args.getArg(1),
			args.getArg(2),
			args.getArg(3),
			args.getArg(4),
			args.getArg(5));
	}	
	assert(!"supporting translation between syscalls for these cpus");
}