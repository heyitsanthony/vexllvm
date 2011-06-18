#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <iostream>
#include <list>

#include "syscallparams.h"
#include "guestmem.h"

#define MAX_SC_TRACE	1000

class Guest;
class GuestCPUState;

class Syscalls
{
public:
	Syscalls(Guest*);
	virtual ~Syscalls();
	virtual uint64_t apply(SyscallParams& args);
	uint64_t apply(void); /* use guest params */
	void print(std::ostream& os) const;
	bool isExit(void) const { return exited; }
private:
	bool interceptSyscall(
		SyscallParams& args,
		GuestMem::Mapping& m, 
		unsigned long& sc_ret);

	Guest				*guest;
	std::list<SyscallParams>	sc_trace;
	uint64_t			sc_seen_c; /* list.size can be O(n) */
	bool				exited;
	GuestCPUState			*cpu_state;
	GuestMem*			mappings;
	const std::string		binary;
	bool				log_syscalls;
	
/* specific architecture handling of syscall could go in another
   class if there wasn't the marshalled syscall option.  so we'll
   dispatch to other functins for platform specific stuff.  these can
   go into separate files if they get big */
#define SYSCALL_HANDLER(arch, call) SYSCALL_HANDLER_TRICK(arch, call, _)

#define SYSCALL_HANDLER_TRICK(arch, call, underbar) \
	bool arch##underbar##call(		\
		SyscallParams&		args,	\
		GuestMem::Mapping&	m,	\
		unsigned long&		sc_ret)
private:
	SYSCALL_HANDLER(AMD64, arch_prctl);
};

#endif
