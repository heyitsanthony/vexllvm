#ifndef ARMCPUSTATE_H
#define ARMCPUSTATE_H

#include <iostream>
#include <stdint.h>
#include <sys/user.h>
#include <assert.h>
#include <map>
#include "guestcpustate.h"

class SyscallParams;

class ARMCPUState : public GuestCPUState
{
public:
	ARMCPUState();
	~ARMCPUState();
	unsigned int byteOffset2ElemIdx(unsigned int off) const;
	void setStackPtr(guest_ptr);
	guest_ptr getStackPtr(void) const;
	void setPC(guest_ptr);
	guest_ptr getPC(void) const;
	SyscallParams getSyscallParams(void) const;
	void setSyscallResult(uint64_t ret);
	uint64_t getExitCode(void) const;

	void setFuncArg(uintptr_t arg_val, unsigned int arg_num);
#ifdef __arm__
	void setRegs(
		const user_regs* regs, const uint8_t* vfpregs,
		void* thread_area);
#endif
	void setThreadPointer(uint32_t v);
	virtual void print(std::ostream& os, const void*) const;

	const char* off2Name(unsigned int off) const;

	virtual unsigned int getRetOff(void) const;
	virtual unsigned int getStackRegOff(void) const;

	const struct guest_ctx_field* getFields(void) const;
};

#endif
