#ifndef MIPS32CPUSTATE_H
#define MIPS32CPUSTATE_H

#include <iostream>
#include <stdint.h>
#include <sys/user.h>
#include <assert.h>
#include <map>
#include "guestcpustate.h"

class SyscallParams;

class MIPS32CPUState : public GuestCPUState
{
public:
typedef std::map<unsigned int, unsigned int> byte2elem_map;

	MIPS32CPUState();
	~MIPS32CPUState();
	unsigned int byteOffset2ElemIdx(unsigned int off) const;
	void setStackPtr(guest_ptr);
	guest_ptr getStackPtr(void) const;
	void setPC(guest_ptr);
	guest_ptr getPC(void) const;
	
	void setFuncArg(uintptr_t arg_val, unsigned int arg_num);
	void print(std::ostream& os, const void*) const;

	const char* off2Name(unsigned int off) const;

	const struct guest_ctx_field* getFields(void) const;

	static const char* abi_spim_scregs[];
};

#define ABI_SPIM_MIPS32	MIPS32CPUState::abi_spim_scregs, "R2", "R4", true

#endif