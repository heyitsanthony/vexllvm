#ifndef I386WINDOWSABI_H
#define I386WINDOWSABI_H

#include "guestabi.h"

class I386WindowsABI : public GuestABI
{
public:
	I386WindowsABI(Guest* g_) : GuestABI(g_) { use_linux_sysenter = false; }
	virtual ~I386WindowsABI(void) {}

	virtual SyscallParams getSyscallParams(void) const;
	virtual void setSyscallResult(uint64_t ret);
	virtual uint64_t getSyscallResult(void) const;
	virtual uint64_t getExitCode(void) const; 
};


#endif