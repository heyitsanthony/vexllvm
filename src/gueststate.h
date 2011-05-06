#ifndef GUESTSTATE_H
#define GUESTSTATE_H

#include <stdint.h>

class ElfImg;
class GuestCPUState;

namespace llvm
{
	class Value;
}

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
	uint64_t addr2Host(uintptr_t guestptr) const;
	const GuestCPUState* getCPUState(void) const { return cpu_state; }
	GuestCPUState* getCPUState(void) { return cpu_state; }
private:
	const ElfImg	*img;
	GuestCPUState	*cpu_state;
	uint8_t		*stack;
};

#endif