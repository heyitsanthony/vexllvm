#ifndef PTSHADOW_H
#define PTSHADOW_H

#include <iostream>
#include <vector>
#include <map>
#include <functional>

#include "guestptr.h"
#include "arch.h"

class SyscallsMarshalled;
class GuestPTImg;
class PTShadow;

typedef std::function<PTShadow*(GuestPTImg*, int pid)> make_ptshadow_t;

class PTShadow
{
public:
	/* fixup type specifies the state that will get clobbered */
	enum FixupDir { FIXUP_NATIVE = -1, FIXUP_NONE = 0, FIXUP_GUEST = 1 };

	PTShadow() {}
	virtual ~PTShadow() {}

	virtual bool isRegMismatch(void) const = 0;
	virtual bool isMatch(void) const = 0;
	virtual FixupDir canFixup(
		const std::vector<InstExtent>& insts,
		bool has_memlog) const = 0;

	virtual void printFPRegs(std::ostream& os) const = 0;
	virtual void printUserRegs(std::ostream& os) const = 0;

	virtual void pushRegisters(void) = 0;

	static PTShadow* create(Arch::Arch, GuestPTImg*, int pid);
	static void registerCPU(Arch::Arch, make_ptshadow_t);

private:
	static std::map<Arch::Arch, make_ptshadow_t> makers;
};

#endif
