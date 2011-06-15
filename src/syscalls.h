#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <iostream>
#include <list>

#include "syscallparams.h"
#include "vexmem.h"

#define MAX_SC_TRACE	1000

class Guest;
class GuestCPUState;

class Syscalls
{
public:
	Syscalls(Guest*, VexMem& mappings);
	virtual ~Syscalls();
	virtual uint64_t apply(SyscallParams& args);
	uint64_t apply(void); /* use guest params */
	void print(std::ostream& os) const;
	bool isExit(void) const { return exited; }
private:
	bool interceptSyscall(
		SyscallParams& args,
		VexMem::Mapping& m, 
		unsigned long& sc_ret);

	Guest				*guest;
	std::list<SyscallParams>	sc_trace;
	uint64_t			sc_seen_c; /* list.size can be O(n) */
	bool				exited;
	GuestCPUState			*cpu_state;
	VexMem&				mappings;
	const std::string		binary;
	bool				log_syscalls;
};

#endif
