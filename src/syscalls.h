#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <iostream>
#include <list>

#include "syscallparams.h"
#include "vexmem.h"

#define MAX_SC_TRACE	1000

class Syscalls
{
public:
	Syscalls(VexMem& mappings);
	virtual ~Syscalls();
	virtual uint64_t apply(SyscallParams& args);
	void print(std::ostream& os) const;
	bool isExit(void) const { return exited; }
private:
	std::list<SyscallParams>	sc_trace;
	uint64_t			sc_seen_c; /* list.size can be O(n) */
	bool				exited;
	VexMem& mappings;
};

#endif
