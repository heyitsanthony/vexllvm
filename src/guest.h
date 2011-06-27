#ifndef GUEST_H
#define GUEST_H

#include <iostream>
#include <stdint.h>
#include <list>
#include "arch.h"
#include "guestmem.h"
#include "syscall/syscallparams.h"

class GuestCPUState;
class GuestSnapshot;

namespace llvm
{
	class Value;
}

/* ties together all state information for guest.
 * 1. register state
 * 2. memory mappings
 * 3. syscall param/result access
 *
 * This is basically an anemic operating system process structure.
 */
class Guest
{
public:
	virtual ~Guest();
	virtual void* getEntryPoint(void) const = 0;
	std::list<GuestMem::Mapping> getMemoryMap(void) const;

	const GuestCPUState* getCPUState(void) const { return cpu_state; }
	GuestCPUState* getCPUState(void) { return cpu_state; }

	SyscallParams getSyscallParams(void) const;
	void setSyscallResult(uint64_t ret);
	virtual std::string getName(void*) const;
	uint64_t getExitCode(void) const;
	void print(std::ostream& os) const;
	virtual Arch::Arch getArch() const = 0;

	const char* getBinaryPath(void) const { return bin_path; }
	const GuestMem* getMem(void) const { assert (mem != NULL); return mem; }
	GuestMem* getMem(void) { assert (mem != NULL); return mem; }

	/* guest has an interface to saving/loading, but all the legwork
	 * is done by guestsnapshot to keep things tidy */
	void save(const char* dirpath = NULL) const;
	static Guest* load(GuestMem* mem, const char* dirpath = NULL);

protected:
	void setBinPath(const char* b);
	Guest(GuestMem* mem, const char* bin_path);

	GuestCPUState	*cpu_state;
	GuestMem	*mem;
	char		*bin_path;
};

#endif