#include <vector>
#include <sstream>
#include "Sugar.h"

#include "syscall/syscalls.h"
#include "guest.h"
#include "guestmem.h"
#include "armcpustate.h"

/* this header loads all of the system headers outside of the namespace */
#include "syscall/translatedsyscall.h"

static GuestMem* g_mem = NULL;
static const GuestMem::Mapping* g_syscall_last_mapping = NULL;
static std::vector<char*> g_to_delete;

namespace ARM {
	/* this will hold the last mapping that was changed during the syscall
	   ... hopefully there is only one... */

	/* we have to define a few platform specific types, non platform
	   specific interface code goes in the translatedutil.h */
	#define TARGET_ARM
	#define TARGET_ABI_BITS 32
	#define TARGET_ABI32 32
	#define abi_long 	int
	#define abi_ulong	unsigned int
	#define target_ulong 	abi_ulong
	#define target_long 	abi_long
	#define PAGE_SIZE 	4096
	#define HOST_PAGE_ALIGN(x) \
		((uintptr_t)x + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)
	#define cpu_to_uname_machine(...)	"armv7l"
	/* apparently, some syscalls depend on if the thing is EABI or not...
	   i.e. truncate64, so we'll just say we are */
	#define ARM_EABI 		1
	
	/* memory mapping requires wrappers based on the host address
	   space limitations */
#ifdef __amd64__
	/* oh poo, this needs to record mappings... they don't right now */
	#define target_mmap(a, l, p, f, fd, o) \
		(abi_long)(uintptr_t)mmap((void*)a, l, p, \
			(f) | MAP_32BIT, fd, o)
	#define mmap_find_vma(s, l)	mmap_find_vma_flags(s, l, MAP_32BIT)
#else
	/* oh poo, this needs to record mappings... they don't right now */
	#define target_mmap(a, l, p, f, fd, o) \
		(abi_long)(uintptr_t)mmap((void*)a, l, p, (f), fd, o)
	#define mmap_find_vma(s, l)	mmap_find_vma_flags(s, l, 0)
#endif	

	/* our implementations of stuff ripped out of the qemu files */
	#include "syscall/translatedutil.h"
	/* generic type conversion utility routines */
	#include "syscall/translatedthunk.h"
	/* terminal io definitions that are plaform dependent */
	#include "cpu/armtermbits.h"
	/* platform dependent signals */
	#include "cpu/armsignal.h"
	/* structure and flag definitions that are platform independent */
	#include "syscall/translatedsyscalldefs.h"
	/* we need load our platform specific stuff as well */
	#include "armsyscallnumbers.h"
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


int Syscalls::translateARMSyscall(int sys_nr) const {
	if((unsigned)sys_nr > ARM::g_guest_to_host_syscalls.size())
		return -1;

	int host_sys_nr = ARM::g_guest_to_host_syscalls[sys_nr];
	return host_sys_nr;
}

std::string Syscalls::getARMSyscallName(int sys_nr) const {
	switch(sys_nr) {
	case ARM_NR_cacheflush:
		return "cacheflush";
	case ARM_NR_set_tls:
		return "settls";
	}
	if((unsigned)sys_nr > ARM::g_guest_syscall_names.size()) {
		std::ostringstream o;
		o << sys_nr;
		return o.str();
	}
	return ARM::g_guest_syscall_names[sys_nr];
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
	case ARM_NR_set_tls:
		if(ARM_set_tls(args, m, sc_ret))
			return sc_ret;
		break;
	case ARM_NR_cacheflush:
		if(ARM_cacheflush(args, m, sc_ret))
			return sc_ret;
		break;
	default:
		break;
	}

	/* if the host and guest are identical, then just pass through */
	if(!force_translation && guest->getArch() == Arch::getHostArch()) {
		return passthroughSyscall(args, m);
	}

	g_mem = mappings;
	sc_ret = ARM::do_syscall(NULL,
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
	
	foreach(it, g_to_delete.begin(), g_to_delete.end())
		delete [] *it;
	g_to_delete.clear();
	
	return sc_ret;
}
SYSCALL_BODY(ARM, cacheflush) {
	return true;
}
SYSCALL_BODY(ARM, set_tls) {
	ARMCPUState* cpu_state = (ARMCPUState*)this->cpu_state;
	cpu_state->setThreadPointer(args.getArg(0));
	//also set the emulation location
	GuestMem::Mapping tls_area;
	mappings->lookupMapping(guest_ptr(0xffff0000), tls_area);
	mappings->mprotect(guest_ptr(0xffff0000), PAGE_SIZE,
		tls_area.cur_prot | PROT_WRITE);
	mappings->write(guest_ptr(0xffff0ff0), (unsigned int)args.getArg(0));
	mappings->mprotect(guest_ptr(0xffff0000), PAGE_SIZE,
		tls_area.cur_prot);
	sc_ret = 0;
	return true;
}
