#ifndef GUESTSTATE_H
#define GUESTSTATE_H

#include <iostream>
#include <stdint.h>
#include "syscallparams.h"

class ElfImg;
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
	GuestState(const ElfImg* img);
	virtual ~GuestState(void);
	llvm::Value* addr2Host(llvm::Value* addr_v) const;
	uint64_t addr2Host(guestptr_t guestptr) const;
	guestptr_t name2guest(std::string& symname) const;
	const GuestCPUState* getCPUState(void) const { return cpu_state; }
	GuestCPUState* getCPUState(void) { return cpu_state; }
	SyscallParams getSyscallParams(void) const;
	void setSyscallResult(uint64_t ret);
	std::string getName(guestptr_t) const;
	uint64_t getExitCode(void) const;
	void print(std::ostream& os) const;
private:
	const ElfImg	*img;
	GuestCPUState	*cpu_state;
	uint8_t		*stack;
};

#endif