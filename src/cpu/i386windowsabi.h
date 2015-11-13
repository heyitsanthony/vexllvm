#ifndef I386WINDOWSABI_H
#define I386WINDOWSABI_H

#include "guestabi.h"

class I386WindowsABI : public RegStrABI
{
public:
	I386WindowsABI(Guest& g_);
	virtual ~I386WindowsABI(void) {}
	SyscallParams getSyscallParams(void) const override;
private:
	unsigned	edx_off;
};


#endif