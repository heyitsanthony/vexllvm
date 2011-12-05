#ifndef PTIMGAMD64_H
#define PTIMGAMD64_H

#include "ptimgarch.h"

struct VexGuestAMD64State;

class PTImgAMD64 : public PTImgArch
{
public:
	bool isMatch(void) const;
	bool doStep(guest_ptr start, guest_ptr end, bool& hit_syscall);
	uintptr_t getSysCallResult() const;
	void stepSysCall(SyscallsMarshalled* sc_m);
	bool breakpointSysCalls(
		const guest_ptr ip_begin,
		const guest_ptr ip_end);

private:
	const VexGuestAMD64State& getVexState(void) const;
	struct user_regs_struct	shadow_reg_cache;

	bool	xchk_eflags;
	bool	fixup_eflags;
};

#endif
