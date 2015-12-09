#ifndef ARMCPUSTATE_H
#define ARMCPUSTATE_H

#include <iostream>
#include <stdint.h>
#include <sys/user.h>
#include <assert.h>
#include <map>
#include "vexcpustate.h"

class SyscallParams;

class ARMCPUState : public VexCPUState
{
public:
	ARMCPUState();
	~ARMCPUState();
	void setStackPtr(guest_ptr);
	guest_ptr getStackPtr(void) const;
	void setPC(guest_ptr);
	guest_ptr getPC(void) const;

#ifdef __arm__
	void setRegs(
		const user_regs* regs, const uint8_t* vfpregs,
		void* thread_area);
#endif
	void setThreadPointer(uint32_t v);
	virtual void print(std::ostream& os, const void*) const;

	virtual unsigned int getRetOff(void) const;
	virtual unsigned int getStackRegOff(void) const;
};

#endif
