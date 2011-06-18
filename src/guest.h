#ifndef GUEST_H
#define GUEST_H

#include <iostream>
#include <stdint.h>
#include <list>
#include "guestmem.h"
#include "syscallparams.h"
#include "elfimg.h"

class GuestCPUState;

namespace llvm
{
	class Value;
}

/* ties together all state information for guest.
 * 1. register state
 * 2. memory mappings
 * 3. syscall param/result access
 */
class Guest
{
public:
	virtual ~Guest(void);
	virtual void* getEntryPoint(void) const = 0;
	std::list<GuestMem::Mapping> getMemoryMap(void) const;

	const GuestCPUState* getCPUState(void) const { return cpu_state; }
	GuestCPUState* getCPUState(void) { return cpu_state; }

	SyscallParams getSyscallParams(void) const;
	void setSyscallResult(uint64_t ret);
	std::string getName(void*) const;
	uint64_t getExitCode(void) const;
	void print(std::ostream& os) const;
	virtual Arch::Arch getArch() const = 0;

	const char* getBinaryPath(void) const { return bin_path; }
	const GuestMem* getMem(void) const { assert (mem != NULL); return mem; }
	GuestMem* getMem(void) { assert (mem != NULL); return mem; }
	static Arch::Arch getHostArch();
protected:
	Guest(const char* bin_path);

	GuestCPUState	*cpu_state;
	GuestMem	*mem;
	char		*bin_path;
};

#endif