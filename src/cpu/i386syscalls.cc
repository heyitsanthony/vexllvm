#include <sys/syscall.h>
#include <sys/errno.h>
#include <sys/prctl.h>
#ifndef __arm__
#include <asm/prctl.h>
#endif
#include <vector>
#include <sstream>
#include "Sugar.h"

#include "guest.h"
#include "cpu/sc_xlate.h"
#include "cpu/i386cpustate.h"


/* this header loads all of the system headers outside of the namespace */
#include "qemu/translatedsyscall.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

static GuestMem* g_mem = NULL;
static std::vector<char*> g_to_delete;

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
	#define mmap_find_vma(s, l)	mmap_find_vma_flags(s, l, MAP_32BIT)

	/* our implementations of stuff ripped out of the qemu files */
	#include "qemu/translatedutil.h"
	/* generic type conversion utility routines */
	#include "qemu/translatedthunk.h"
	/* terminal io definitions that are plaform dependent */
	#include "qemu/i386termbits.h"
	/* platform dependent signals */
	#include "qemu/i386signal.h"
	/* structure and flag definitions that are platform independent */
	#include "qemu/translatedsyscalldefs.h"
	/* we need load our platform specific stuff as well */
	#include "qemu/i386syscallnumbers.h"
	/* this constructs all of our syscall translation code */
	#include "qemu/translatedsyscall.c"
	/* this constructs all of our syscall translation code */
	#include "qemu/translatedsignal.c"
	/* this has some tables and such used for type conversion */
	#include "qemu/translatedthunk.c"
	
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


int I386SyscallXlate::translateSyscall(int sys_nr) const {
	if((unsigned)sys_nr > I386::g_guest_to_host_syscalls.size())
		return -1;

	int host_sys_nr = I386::g_guest_to_host_syscalls[sys_nr];
	return host_sys_nr;
}

std::string I386SyscallXlate::getSyscallName(int sys_nr) const {
	if((unsigned)sys_nr > I386::g_guest_syscall_names.size()) {
		std::ostringstream o;
		o << sys_nr;
		return o.str();
	}

	return I386::g_guest_syscall_names[sys_nr];
}

SYSCALL_BODY(I386, mmap2)
{
	guest_ptr m;
	sc_ret = g.getMem()->mmap(m,
		guest_ptr(args.getArg(0)),
		args.getArg(1),
		args.getArg(2),
		args.getArg(3),
		args.getArg(4),
		args.getArg(5) * PAGE_SIZE);
	if(sc_ret == 0)
		sc_ret = m;
	return true;
}

uintptr_t I386SyscallXlate::apply(Guest& g, SyscallParams& args)
{
	uintptr_t sc_ret = 0;

	sc_ret = ~sc_ret;

	/* special syscalls that we handle per arch, these
	   generally supersede any pass through or translated
	   behaviors, but they can just alter the args and let
	   the other mechanisms finish the job */
	switch (args.getSyscall()) {
	case TARGET_NR_mmap2:
		if(I386_mmap2(g, args, (unsigned long&)sc_ret))
			return sc_ret;
		break;
	default:
		break;
	}

	if (tryPassthrough(g, args, sc_ret)) return sc_ret;
	
	g_mem = g.getMem();
	sc_ret = I386::do_syscall(NULL,
		args.getSyscall(),
		args.getArg(0),
		args.getArg(1),
		args.getArg(2),
		args.getArg(3),
		args.getArg(4),
		args.getArg(5));
			
	foreach(it, g_to_delete.begin(), g_to_delete.end())
		delete [] *it;
	g_to_delete.clear();
	
	return sc_ret;
}