#ifndef SPIMSYSCALLS_H
#define SPIMSYSCALLS_H

#include "syscalls.h"

class SPIMSyscalls : public Syscalls
{
public:
	SPIMSyscalls(Guest* gs) : Syscalls(gs) {}
	virtual ~SPIMSyscalls(void) {}
	virtual uint64_t apply(SyscallParams& args);
};

#endif
