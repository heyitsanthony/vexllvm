#ifndef GUESTSTATE_H
#define GUESTSTATE_H

#include <iostream>
#include <stdint.h>
#include "syscallparams.h"

class GuestCPUState;

namespace llvm
{
	class Value;
}

typedef uint64_t guestptr_t;

/* TODO: make base class */
/* ties together all state information for guest.
 * 1. register state
 * 2. memory mappings
 * This might have to be hacked for KLEE
 */
class GuestState
{
public:
	GuestState(void);
	virtual ~GuestState(void);
	virtual llvm::Value* addrVal2Host(llvm::Value* addr_v) const = 0;
	virtual uint64_t addr2Host(guestptr_t guestptr) const = 0;
	virtual guestptr_t name2guest(const char*) const = 0;
	virtual void* getEntryPoint(void) const = 0;

	const GuestCPUState* getCPUState(void) const { return cpu_state; }
	GuestCPUState* getCPUState(void) { return cpu_state; }
	SyscallParams getSyscallParams(void) const;
	void setSyscallResult(uint64_t ret);
	std::string getName(guestptr_t) const;
	uint64_t getExitCode(void) const;
	void print(std::ostream& os) const;

	/* copy command line state into state */
	void setArgv(unsigned int argc, const char* argv[]);
private:
	GuestCPUState	*cpu_state;
	uint8_t		*stack;
};

#endif