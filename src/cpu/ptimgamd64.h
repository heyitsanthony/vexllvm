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

	bool isMatch(void) const;
	bool isRegMismatch(void) const;
	bool doStep(guest_ptr start, guest_ptr end, bool& hit_syscall);
	uintptr_t getSysCallResult() const;
	
	void stepSysCall(SyscallsMarshalled* sc_m);
	void ignoreSysCall(void);

	bool breakpointSysCalls(
		const guest_ptr ip_begin,
		const guest_ptr ip_end);

	GuestPTImg::FixupDir canFixup(
		const std::vector<InstExtent>& insts,
		bool has_memlog) const;

	void getRegs(user_regs_struct&) const;
	void setRegs(const user_regs_struct& regs);
	guest_ptr getPC(void);
	guest_ptr getStackPtr(void) const;
	virtual void pushRegisters(void);

	void resetBreakpoint(guest_ptr addr, long v);
	void printFPRegs(std::ostream& os) const;
	void printUserRegs(std::ostream& os) const;
	void slurpRegisters(void);

	long setBreakpoint(guest_ptr addr);
	guest_ptr undoBreakpoint(void);
	void revokeRegs(void);

	virtual void restore(void);

	virtual uint64_t dispatchSysCall(const SyscallParams& sp);
protected:
	virtual void waitForSingleStep(void);

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


	struct user_regs_struct& getRegs(void) const;
	const VexGuestAMD64State& getVexState(void) const;
	VexGuestAMD64State& getVexState(void);

	// don't reference directly-- use getRegs() to
	// ensure most current version
	mutable struct user_regs_struct	shadow_reg_cache;
	mutable bool	recent_shadow;

	bool	xchk_eflags;
	bool	fixup_eflags;

	/* caches check for opcodes */
	guest_ptr	chk_addr;
	long		chk_opcode;
};

#endif
