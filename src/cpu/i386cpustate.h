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
	I386CPUState();
	~I386CPUState();
	void setStackPtr(guest_ptr) override;
	guest_ptr getStackPtr(void) const override;
	void setPC(guest_ptr) override;
	guest_ptr getPC(void) const override;

	void print(std::ostream& os, const void*) const override;
	unsigned int getStackRegOff(void) const override;

	void noteRegion(const char* name, guest_ptr addr) override;
};

#endif
