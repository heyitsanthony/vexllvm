#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "syscallparams.h"

class Syscalls
{
public:
	Syscalls();
	virtual ~Syscalls();
	uint64_t apply(const SyscallParams& args);
private:
};

#endif
