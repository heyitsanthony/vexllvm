#ifndef PTIMGARCH_H
#define PTIMGARCH_H

#include "guestptimg.h"

class PTImgArch
{
public:
	virtual bool isRegMismatch(void) const = 0;
	virtual void loadRegs(void) = 0;
	virtual bool fixup(const std::vector<InstExtent>& insts) = 0;
	virtual void printFPRegs(std::ostream& os) const = 0;
	virtual void printUserRegs(std::ostream& os) const = 0;
	virtual guest_ptr getStackPtr(void) const = 0;
	virtual void slurpRegisters(void) = 0;
	virtual void stepSysCall(SyscallsMarshalled* sc_m) = 0;

	virtual long setBreakpoint(guest_ptr addr) = 0;
	virtual void resetBreakpoint(guest_ptr addr) = 0;
	virtual guest_ptr undoBreakpoint() = 0;
	virtual bool doStep(
		guest_ptr start, guest_ptr end, bool& hit_syscall) = 0;

	virtual guest_ptr getPC(void) = 0;

	virtual bool canFixup(
		const std::vector<InstExtent>& insts,
		bool has_memlog) = 0;

	virtual bool isMatch(void) const;
	virtual uintptr_t getSysCallResult() const = 0;

	virtual bool breakpointSysCalls(
		const guest_ptr ip_begin,
		const guest_ptr ip_end) = 0;

protected:
	PTImgArch(GuestPTImg* in_gs, int in_pid)
	: gs(in_gs)
	, child_pid(in_pid) {}

	GuestPTImg	*gs;
	int		child_pid;
private:
};

#endif
