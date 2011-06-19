#ifndef AMD64CPUSTATE_H
#define AMD64CPUSTATE_H

#include <iostream>
#include <stdint.h>
#include "syscallparams.h"
#include <sys/user.h>
#include <assert.h>
#include <map>
#include "guestcpustate.h"

class AMD64CPUState : public GuestCPUState
{
public:
	AMD64CPUState();
	~AMD64CPUState();
	unsigned int byteOffset2ElemIdx(unsigned int off) const;
	void setStackPtr(void*);
	void* getStackPtr(void) const;
	void setPC(void*);
	void* getPC(void) const;
	SyscallParams getSyscallParams(void) const;
	void setSyscallResult(uint64_t ret);
	uint64_t getExitCode(void) const;

	void setFuncArg(uintptr_t arg_val, unsigned int arg_num);
#ifdef __amd64__
	void setRegs(const user_regs_struct& regs, 
		const user_fpregs_struct& fpregs);
#endif
	void print(std::ostream& os) const;

	GuestTLS* getTLS(void) { return tls; }
	const GuestTLS* getTLS(void) const { return tls; }
	void setTLS(GuestTLS* tls);

	void setFSBase(uintptr_t base);
	uintptr_t getFSBase() const;

	const char* off2Name(unsigned int off) const;
protected:
	void mkRegCtx(void);
private:
	GuestTLS	*tls;
};

#endif
