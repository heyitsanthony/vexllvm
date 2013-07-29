#ifndef PTIMGARM_H
#define PTIMGARM_H

#include <sys/user.h>
#include "ptimgarch.h"

extern "C" {
#include <valgrind/libvex_guest_arm.h>
}

class PTImgARM : public PTImgArch
{
public:
	PTImgARM(GuestPTImg* gs, int in_pid);
	virtual ~PTImgARM();

	bool isMatch(void) const;
	bool doStep(guest_ptr start, guest_ptr end, bool& hit_syscall);
	uintptr_t getSysCallResult() const;

	bool isRegMismatch() const;
	void printFPRegs(std::ostream&) const;
	void printUserRegs(std::ostream&) const;
	guest_ptr getStackPtr() const;
	void slurpRegisters();
	void stepSysCall(SyscallsMarshalled*);
	long int setBreakpoint(guest_ptr);
	guest_ptr undoBreakpoint();
	guest_ptr getPC();
	GuestPTImg::FixupDir canFixup(
		const std::vector<std::pair<guest_ptr, unsigned char> >&,
		bool has_memlog) const;
	bool breakpointSysCalls(guest_ptr, guest_ptr);
	void revokeRegs();
	void resetBreakpoint(guest_ptr addr, long v);

private:
	const VexGuestARMState& getVexState(void) const;
	struct user_regs	shadow_reg_cache;
};

#endif
