#include "syscalls.h"
#include "guest.h"
#include "guestmem.h"

/* this header loads all of the system headers outside of the namespace */
#include "translatedsyscall.h"

static GuestMem::Mapping* g_syscall_last_mapping = NULL;

namespace ARM {
	/* this will hold the last mapping that was changed during the syscall
	   ... hopefully there is only one... */

	/* we have to define a few platform specific types, non platform
	   specific interface code goes in the translatedutil.h */
	#define TARGET_ARM
	#define abi_long 	int
	#define abi_ulong	unsigned int
	#define target_ulong 	abi_ulong
	#define target_long 	abi_long
	#define PAGE_SIZE 4096
	#define HOST_PAGE_ALIGN(x) \
		((uintptr_t)x + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)
	#define TARGET_ABI_BITS 32

	/* memory mapping requires wrappers based on the host address
	   space limitations */
#ifdef __amd64__
	#define target_mmap(a, l, p, f, fd, o) \
		(abi_long)(uintptr_t)mmap((void*)a, l, p, \
			(f) | MAP_32BIT, fd, o)
	#define mmap_find_vma(s, l)	mmap_find_vma_flags(s, l, MAP_32BIT)
#else
	#define target_mmap(a, l, p, f, fd, o) \
		(abi_long)(uintptr_t)mmap((void*)a, l, p, (f), fd, o)
	#define mmap_find_vma(s, l)	mmap_find_vma_flags(s, l, 0)
#endif

	/* our implementations of stuff ripped out of the qemu files */
	#include "translatedutil.h"
	/* terminal io definitions that are plaform dependent */
	#include "armtermbits.h"
	/* structure and flag definitions that are platform independent */
	#include "translatedsyscalldefs.h"
	/* we need load our platform specific stuff as well */
	#include "armsyscallnumbers.h"
	/* this constructs all of our syscall translation code */
	/* #include "translatedsyscall.c" */

}

int Syscalls::translateARMSyscall(int sys_nr) {
	assert(guest->getArch() == Arch::getHostArch());
	return sys_nr;
}

uintptr_t Syscalls::applyARMSyscall(
	SyscallParams& args,
	GuestMem::Mapping& m)
{
	uintptr_t sc_ret = ~0ULL;

	/* special syscalls that we handle per arch, these
	   generally supersede any pass through or translated 
	   behaviors, but they can just alter the args and let
	   the other mechanisms finish the job */
	switch (args.getSyscall()) {
	default:
		break;
	}

	/* if the host and guest are identical, then just pass through */
	if(guest->getArch() == Arch::getHostArch()) {
		return passthroughSyscall(args, m);
	}

	uintptr_t sc_res;
	/*
	uintptr_t sc_res = ARM::do_syscall(NULL,
		args.getSyscall(),
		args.getArg(0),
		args.getArg(1),
		args.getArg(2),
		args.getArg(3),
		args.getArg(4),
		args.getArg(5));
		
	if(g_syscall_last_mapping) {
		m = *g_syscall_last_mapping;
		g_syscall_last_mapping = NULL;
	}
	
	return sc_ret;
	*/

	std::cerr << "syscall(" << args.getSyscall() << ", "
		<< (void*)args.getArg(0) << ", "
		<< (void*)args.getArg(1) << ", "
		<< (void*)args.getArg(2) << ", "
		<< (void*)args.getArg(3) << ", "
		<< (void*)args.getArg(4) << ", ...) => ???"
		<< std::endl;
	assert(!"supporting translation between syscalls for these cpus");

	return sc_ret;
}