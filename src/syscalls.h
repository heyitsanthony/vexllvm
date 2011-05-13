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
	uint64_t apply(const SyscallParams& args);
	void print(std::ostream& os) const;
private:
	std::list<uint64_t>	call_trace;
};

#endif
