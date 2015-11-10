#ifndef PTCPUSTATE_H
#define PTCPUSTATE_H

#include <unistd.h>
#include "guestptr.h"
#include "guestcpustate.h"

// a cpu state that is backed by a ptraced process
class PTCPUState : public GuestCPUState
{
public:
	int waitForSingleStep(void);
	void copyIn(guest_ptr dst, const void* src, unsigned int bytes) const;
	void revokeRegs(void) { recent_shadow = false; }
	void resetBreakpoint(guest_ptr addr, long v);

	virtual void ignoreSysCall(void) = 0;
	virtual uint64_t dispatchSysCall(const SyscallParams& sp, int& wss) = 0;
	virtual void loadRegs(void) = 0;
	virtual guest_ptr undoBreakpoint(void) = 0;
	virtual long setBreakpoint(guest_ptr addr) = 0;
	virtual bool isSyscallOp(guest_ptr addr, long v) const = 0;

	virtual uintptr_t getSysCallResult() const = 0;

	static void registerCPUs(pid_t);

protected:
	PTCPUState(pid_t in_pid)
		: pid(in_pid)
		, recent_shadow(false)
	{
		assert(pid != 0);
	}

	pid_t		pid;
	mutable bool	recent_shadow;
};

#endif