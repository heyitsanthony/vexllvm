#ifndef PTCTL_H
#define PTCTL_H

#include "guestptr.h"
#include "guestptimg.h"

class PTImgArch;
class GUestPTImg;
class SyscallsMarshalled;

class PTCtl
{
public:
	PTCtl(GuestPTImg& _gpi);
	virtual ~PTCtl() {}

	void pushRegisters(void);
	void pushPage(guest_ptr p, GuestMem* = nullptr);

	guest_ptr stepToBreakpoint(void);
	void stepThroughBounds(guest_ptr start, guest_ptr end);
	void ignoreSysCall(void);
	void stepSysCall(SyscallsMarshalled* sc);
	guest_ptr continueForwardWithBounds(guest_ptr start, guest_ptr end);

	bool breakpointSysCalls(guest_ptr ip_begin, guest_ptr ip_end);

	uintptr_t getSysCallResult(void) const;

protected:
	int		pid;

private:
	GuestPTImg	&gpi;
	bool		hit_syscall;
	bool		log_steps;
	uint64_t	bp_steps;
};

#endif
