#ifndef MIPS32CPUSTATE_H
#define MIPS32CPUSTATE_H

#include <iostream>
#include <stdint.h>
#include <sys/user.h>
#include <assert.h>
#include <map>
#include "vexcpustate.h"

class SyscallParams;

class MIPS32CPUState : public VexCPUState
{
public:
	MIPS32CPUState();
	~MIPS32CPUState();
	void setStackPtr(guest_ptr);
	guest_ptr getStackPtr(void) const;
	void setPC(guest_ptr);
	guest_ptr getPC(void) const;
	
	void print(std::ostream& os, const void*) const;

	const char* off2Name(unsigned int off) const;

	const struct guest_ctx_field* getFields(void) const;

	static const char* abi_spim_scregs[];
};

#define ABI_SPIM_MIPS32	MIPS32CPUState::abi_spim_scregs, "R2", "R4", true

#endif
