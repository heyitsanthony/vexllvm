#ifndef PTSHADOWAMD64_H
#define PTSHADOWAMD64_H

#include "cpu/ptimgamd64.h"
#include "ptshadow.h"

extern "C" {
#include <valgrind/libvex_guest_amd64.h>
}


class PTShadowAMD64 : public PTImgAMD64, public PTShadow
{
public:
	PTShadowAMD64(GuestPTImg* gs, int in_pid);

	// shadow stuff
	void printUserRegs(std::ostream& os) const override;
	void printFPRegs(std::ostream& os) const override;
	void slurpRegisters(void) override;
	void pushRegisters(void) override;

	FixupDir canFixup(
		const std::vector<InstExtent>& insts,
		bool has_memlog) const override;

	bool isRegMismatch(void) const override;
	bool isMatch(void) const override;

	// ptimg stuff
	bool filterSysCall(void) override;
	bool doStep(guest_ptr start, guest_ptr end, bool& hit_syscall) override;
	void restore(void) override;

	static void vex2ptrace(
		const VexGuestAMD64State&,
		struct user_regs_struct&,
		struct user_fpregs_struct&);

	static void ptrace2vex(
		const struct user_regs_struct&,
		const struct user_fpregs_struct&,
		VexGuestAMD64State&);

protected:
	void vex2ptrace(void);
	void ptrace2vex(void);

	// ptimg
	void handleBadProgress(void) override;

private:
	const VexGuestAMD64State& getVexState(void) const;
	VexGuestAMD64State& getVexState(void);

	bool	xchk_eflags;
	bool	fixup_eflags;
};
#endif
