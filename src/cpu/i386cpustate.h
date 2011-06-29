#ifndef I386CPUSTATE_H
#define I386CPUSTATE_H

#include <iostream>
#include <stdint.h>
#include <sys/user.h>
#include <assert.h>
#include <map>
#include "guestcpustate.h"

class SyscallParams;

class I386CPUState : public GuestCPUState
{
public:
typedef std::map<unsigned int, unsigned int> byte2elem_map;

	I386CPUState();
	~I386CPUState();
	unsigned int byteOffset2ElemIdx(unsigned int off) const;
	void setStackPtr(guest_ptr);
	guest_ptr getStackPtr(void) const;
	void setPC(guest_ptr);
	guest_ptr getPC(void) const;
	SyscallParams getSyscallParams(void) const;
	void setSyscallResult(uint64_t ret);
	uint64_t getExitCode(void) const;
	
	void setFuncArg(uintptr_t arg_val, unsigned int arg_num);
#ifdef __i386__
	void setRegs(const user_regs_struct& regs, 
		const user_fpregs_struct& fpregs);
#endif
	void print(std::ostream& os, const void*) const;

	const char* off2Name(unsigned int off) const;
protected:
	void mkRegCtx(void);
private:
};

#endif
