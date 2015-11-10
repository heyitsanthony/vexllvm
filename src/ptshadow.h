#ifndef PTSHADOW_H
#define PTSHADOW_H

#include "guestptimg.h"

class SyscallsMarshalled;

class PTShadow
{
public:
	PTShadow() {}
	virtual ~PTShadow() {}

	virtual bool isRegMismatch(void) const = 0;
	virtual void printFPRegs(std::ostream& os) const = 0;
	virtual void printUserRegs(std::ostream& os) const = 0;
	virtual void slurpRegisters(void) = 0;
	virtual void pushRegisters(void) { assert (0 == 1 && "STUB"); }
	virtual void stepSysCall(SyscallsMarshalled* sc_m) = 0;
	virtual void ignoreSysCall(void) { assert (0 == 1 && "STUB"); }

	virtual GuestPTImg::FixupDir canFixup(
		const std::vector<InstExtent>& insts,
		bool has_memlog) const = 0;

	virtual void fixupRegsPreSyscall(void)
	{ assert (0 == 1 && "Not implemented for this arch"); }

	virtual bool isMatch(void) const = 0;
};

#endif
