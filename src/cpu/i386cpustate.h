#ifndef I386CPUSTATE_H
#define I386CPUSTATE_H

#include <iostream>
#include <stdint.h>
#include <sys/user.h>
#include <assert.h>
#include <map>
#include "vexcpustate.h"

class SyscallParams;

class I386CPUState : public VexCPUState
{
public:
typedef std::map<unsigned int, unsigned int> byte2elem_map;

	I386CPUState();
	~I386CPUState();
	void setStackPtr(guest_ptr) override;
	guest_ptr getStackPtr(void) const override;
	void setPC(guest_ptr) override;
	guest_ptr getPC(void) const override;

	void print(std::ostream& os, const void*) const override;
	const char* off2Name(unsigned int off) const override;
	const struct guest_ctx_field* getFields(void) const override;
	unsigned int getStackRegOff(void) const override;

	void noteRegion(const char* name, guest_ptr addr) override;

	static const char* abi_linux_scregs[];
};

#define	ABI_LINUX_I386	I386CPUState::abi_linux_scregs, \
	"EAX",	/* syscall result */	\
	"EBX",	/* get exit code */	\
	true	/* 32-bit */



#endif
