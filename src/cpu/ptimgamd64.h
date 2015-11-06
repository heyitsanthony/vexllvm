#ifndef PTIMGAMD64_H
#define PTIMGAMD64_H

#include <sys/user.h>
#include "ptimgarch.h"

extern "C" {
#include <valgrind/libvex_guest_amd64.h>
}

class PTImgAMD64 : public PTImgArch
{
public:
	PTImgAMD64(GuestPTImg* gs, int in_pid);
	virtual ~PTImgAMD64(void) {}

	void setPID(int in_pid) override;
	bool isMatch(void) const override;
	bool isRegMismatch(void) const override;
	bool doStep(guest_ptr start, guest_ptr end, bool& hit_syscall) override;

	bool breakpointSysCalls(
		const guest_ptr ip_begin,
		const guest_ptr ip_end) override;

	GuestPTImg::FixupDir canFixup(
		const std::vector<InstExtent>& insts,
		bool has_memlog) const override;
	void fixupRegsPreSyscall(void) override;

	void stepSysCall(SyscallsMarshalled* sc_m) override;
	void ignoreSysCall(void) override;
	void pushRegisters(void) override;
	void slurpRegisters(void) override;
	void restore(void) override;

	void printFPRegs(std::ostream& os) const override;
	void printUserRegs(std::ostream& os) const override;


	static void ptrace2vex(
		const user_regs_struct& urs,
		const user_fpregs_struct& fpregs,
		VexGuestAMD64State& vs);

	static void vex2ptrace(
		const VexGuestAMD64State& vs,
		user_regs_struct& urs,
		user_fpregs_struct& fpregs);

	void getRegs(user_regs_struct&) const;
	void setRegs(const user_regs_struct&);
private:
	bool isPushF(void);
	bool isOnCPUID(void);
	bool isOnRDTSC(void);
	bool isOnSysCall(void);
	long getInsOp(void);
	long getInsOp(long pc);

	bool filterSysCall(
		const VexGuestAMD64State& state,
		user_regs_struct& regs);

	const VexGuestAMD64State& getVexState(void) const;
	VexGuestAMD64State& getVexState(void);

	bool	xchk_eflags;
	bool	fixup_eflags;

	/* caches check for opcodes */
	guest_ptr	chk_addr;
	long		chk_opcode;
};

#endif
