#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <iostream>
#include <list>

#include "syscallparams.h"
#include "vexmem.h"

class Syscalls
{
public:
	Syscalls(VexMem& mappings);
	virtual ~Syscalls();
	virtual uint64_t apply(SyscallParams& args);
	void print(std::ostream& os) const;
	bool isExit(void) const { return exited; }
private:
	std::list<SyscallParams>	call_trace;
	bool				exited;
	VexMem& mappings;
};

#endif
