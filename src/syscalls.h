#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <iostream>
#include <list>

#include "syscallparams.h"

class Syscalls
{
public:
	Syscalls();
	virtual ~Syscalls();
	virtual uint64_t apply(const SyscallParams& args);
	void print(std::ostream& os) const;
	bool isExit(void) const { return exited; }
private:
	std::list<SyscallParams>	call_trace;
	bool				exited;
};

#endif
