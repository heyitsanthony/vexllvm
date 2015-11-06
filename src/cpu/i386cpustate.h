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
	void setStackPtr(guest_ptr);
	guest_ptr getStackPtr(void) const;
	void setPC(guest_ptr);
	guest_ptr getPC(void) const;

	void setGDT(guest_ptr);
	void setLDT(guest_ptr);
	
	void print(std::ostream& os, const void*) const;

	const char* off2Name(unsigned int off) const;

	const struct guest_ctx_field* getFields(void) const;

	virtual unsigned int getStackRegOff(void) const;

	static const char* abi_linux_scregs[];
};

#define	ABI_LINUX_I386	I386CPUState::abi_linux_scregs, \
	"EAX",	/* syscall result */	\
	"EBX",	/* get exit code */	\
	true	/* 32-bit */



#endif
