#include <sys/syscall.h>
#include <sys/errno.h>
#include <sys/prctl.h>
#include <asm/prctl.h> 
#include "guestmem.h"
#include <vector>
#include <sstream>
#include "Sugar.h"

#include "guest.h"
#include "syscall/syscalls.h"
#include "i386cpustate.h"


/* this header loads all of the system headers outside of the namespace */
#include "syscall/translatedsyscall.h"

static GuestMem::Mapping* g_syscall_last_mapping = NULL;

namespace I386 {
	/* this will hold the last mapping that was changed during the syscall
	   ... hopefully there is only one... */

	/* we have to define a few platform specific types, non platform
	   specific interface code goes in the translatedutil.h */
	#define TARGET_I386
	#define TARGET_ABI_BITS 32
	#define TARGET_ABI32
	#define abi_long 	int
	#define abi_ulong	unsigned int
	#define target_ulong 	abi_ulong
	#define target_long 	abi_long
	#define HOST_PAGE_ALIGN(x) \
		((uintptr_t)x + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)
	#define cpu_to_uname_machine(...)	"i386"
	/* Maximum number of LDT entries supported. */
	#define TARGET_LDT_ENTRIES	8192
	/* The size of each LDT entry. */
	#define TARGET_LDT_ENTRY_SIZE	8
		
	/* memory mapping requires wrappers based on the host address
	   space limitations */
	/* oh poo, this needs to record mappings... they don't right now */
	#define target_mmap(a, l, p, f, fd, o) \
		(abi_long)(uintptr_t)mmap((void*)a, l, p, f, fd, o)
	#define mmap_find_vma(s, l)	mmap_find_vma_flags(s, l, MAP_32BIT)

	/* our implementations of stuff ripped out of the qemu files */
	#include "syscall/translatedutil.h"
	/* generic type conversion utility routines */
	#include "syscall/translatedthunk.h"
	/* terminal io definitions that are plaform dependent */
	#include "cpu/i386termbits.h"
	/* platform dependent signals */
	#include "cpu/i386signal.h"
	/* structure and flag definitions that are platform independent */
	#include "syscall/translatedsyscalldefs.h"
	/* we need load our platform specific stuff as well */
	#include "cpu/i386syscallnumbers.h"
	/* this constructs all of our syscall translation code */
	#include "syscall/translatedsyscall.c"
	/* this constructs all of our syscall translation code */
	#include "syscall/translatedsignal.c"
	/* this has some tables and such used for type conversion */
	#include "syscall/translatedthunk.c"
	
	std::vector<int> g_host_to_guest_syscalls(512);
	std::vector<int> g_guest_to_host_syscalls(512);
	std::vector<std::string> g_guest_syscall_names(512);
	bool syscall_mapping_init() {
		foreach(it, g_host_to_guest_syscalls.begin(),
			g_host_to_guest_syscalls.end()) *it = -1;
		foreach(it, g_guest_to_host_syscalls.begin(),
			g_guest_to_host_syscalls.end()) *it = -1;
		for(unsigned sys_nr = 0; 
			sys_nr < g_guest_syscall_names.size(); ++sys_nr) 
		{
			std::ostringstream o;
			o << sys_nr;
			g_guest_syscall_names[sys_nr] = o.str();
		}
		#define SYSCALL_RELATION(name, host, guest) 	\
			g_host_to_guest_syscalls[host] = guest;	\
			g_guest_to_host_syscalls[guest] = host;
		#define GUEST_SYSCALL(name, guest) 	\
			g_guest_syscall_names[guest] = #name;
		#include "syscall/syscallsmapping.h"
		return true;
	}
	bool dummy_syscall_mapping = syscall_mapping_init();
}


int Syscalls::translateI386Syscall(int sys_nr) const {
	if((unsigned)sys_nr > I386::g_guest_to_host_syscalls.size())
		return -1;

	int host_sys_nr = I386::g_guest_to_host_syscalls[sys_nr];
	return host_sys_nr;
}

std::string Syscalls::getI386SyscallName(int sys_nr) const {
	if((unsigned)sys_nr > I386::g_guest_syscall_names.size()) {
		std::ostringstream o;
		o << sys_nr;
		return o.str();
	}

	return I386::g_guest_syscall_names[sys_nr];
}

uintptr_t Syscalls::applyI386Syscall(
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
	if(!force_translation && guest->getArch() == Arch::getHostArch()) {
		return passthroughSyscall(args, m);
	}
	
	sc_ret = I386::do_syscall(NULL,
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
}
