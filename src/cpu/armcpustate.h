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
	void setRegs(const user& regs, const user_fp& fpregs, 
		const user_vfp& vfpregs) {
#endif
	void setThreadPointer(uint32_t v);
	void print(std::ostream& os) const;

	const char* off2Name(unsigned int off) const;
protected:
	void mkRegCtx(void);
};

#endif
