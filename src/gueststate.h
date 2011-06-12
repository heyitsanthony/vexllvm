#ifndef GUESTSTATE_H
#define GUESTSTATE_H

#include <iostream>
#include <stdint.h>
#include <list>
#include "syscallparams.h"

class GuestCPUState;

namespace llvm
{
	class Value;
}

typedef uint64_t guestptr_t;

/* extent of guest memory range; may need to extend later so 
 * getData works on non-identity mappings */
class GuestMemoryRange
{
public:
	GuestMemoryRange(void* in_base, unsigned int in_sz, bool in_is_stack)
	: base(in_base), sz(in_sz), is_stack(in_is_stack) {}
	virtual ~GuestMemoryRange(void) {}
	const void* getData(void) const { return base; }
	const void* getGuestAddr(void) const { return base; }
	unsigned int getBytes(void) { return sz; }
	bool isStack(void) const { return is_stack; }
private:
	void		*base;
	unsigned int	sz;
	bool		is_stack;
};


/* ties together all state information for guest.
 * 1. register state
 * 2. memory mappings
 * 3. syscall param/result access
 */
class GuestState
{
public:
	virtual ~GuestState(void);
	virtual llvm::Value* addrVal2Host(llvm::Value* addr_v) const = 0;
	virtual uint64_t addr2Host(guestptr_t guestptr) const = 0;
	virtual guestptr_t name2guest(const char*) const = 0;
	virtual void* getEntryPoint(void) const = 0;
	virtual std::list<GuestMemoryRange*> getMemoryMap(void) const = 0;

	const GuestCPUState* getCPUState(void) const { return cpu_state; }
	GuestCPUState* getCPUState(void) { return cpu_state; }

	SyscallParams getSyscallParams(void) const;
	void setSyscallResult(uint64_t ret);
	std::string getName(guestptr_t) const;
	uint64_t getExitCode(void) const;
	void print(std::ostream& os) const;

	const char* getBinaryPath(void) const { return bin_path; }

protected:
	GuestState(const char* bin_path);

	GuestCPUState	*cpu_state;
	char		*bin_path;
};

#endif