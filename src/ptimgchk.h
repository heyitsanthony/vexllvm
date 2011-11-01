#ifndef PTIMGCHK_H
#define PTIMGCHK_H

#include <sys/types.h>
#include <sys/user.h>
#include "guestptimg.h"

class MemLog;
class SyscallsMarshalled;

class PTImgChk : public GuestPTImg
{
public:
	PTImgChk(int argc, char* const argv[], char* const envp[]);
	virtual ~PTImgChk();

	void stepThroughBounds(
		guest_ptr start, guest_ptr end,
		const VexGuestAMD64State& state);
	guest_ptr stepToBreakpoint(void);

	void stepSysCall(SyscallsMarshalled* sc, const VexGuestAMD64State& st);
	guest_ptr continueForwardWithBounds(
		guest_ptr start, guest_ptr end,
		const VexGuestAMD64State& state);
	void printSubservient(
		std::ostream& os, 
		const VexGuestAMD64State& ref) const;
	void stackTraceSubservient(std::ostream& os, 
		guest_ptr b = guest_ptr(0), guest_ptr e = guest_ptr(0));
	void printTraceStats(std::ostream& os);

	bool isMatch(const VexGuestAMD64State& state) const;
	bool isMatchMemLog() const;

	bool fixup(guest_ptr ip_begin, guest_ptr ip_end);

	bool breakpointSysCalls(guest_ptr ip_begin, guest_ptr ip_end);

	void getRegs(user_regs_struct& regs) const;
	void setRegs(const user_regs_struct& regs);
	uintptr_t getSysCallResult() const;

	void resetBreakpoint(guest_ptr x) {
		GuestPTImg::resetBreakpoint(child_pid, x); }
	void setBreakpoint(guest_ptr x) {
		GuestPTImg::setBreakpoint(child_pid, x); }

	MemLog* getMemLog(void) { return mem_log; }
protected:
	virtual void handleChild(pid_t pid);
	bool filterSysCall(
		const VexGuestAMD64State& state,
		user_regs_struct& regs);

private:
	bool doStep(
		guest_ptr start, guest_ptr end,
		user_regs_struct& regs,
		const VexGuestAMD64State& state);
	void waitForSingleStep(void);

	bool isRegMismatch(
		const VexGuestAMD64State& state,
		const user_regs_struct& regs) const;
	long getInsOp(const user_regs_struct& regs);
	long getInsOp(long rip);
	bool isOnSysCall(const user_regs_struct& regs);
	bool isOnRDTSC(const user_regs_struct& regs);
	bool isOnCPUID(const user_regs_struct& regs);
	bool isPushF(const user_regs_struct& regs);

	void printUserRegs(
		std::ostream& os, 
		user_regs_struct& regs,
		const VexGuestAMD64State& ref) const;

	void printFPRegs(
		std::ostream& os,
		user_fpregs_struct& fpregs,
		const VexGuestAMD64State& ref) const;

	void printMemory(std::ostream& os) const;

	void copyIn(guest_ptr dst, const void* src, unsigned int bytes);

	pid_t		child_pid;
	
	uint64_t	steps;
	uint64_t	bp_steps;
	uint64_t	blocks;

	bool		log_steps;
	unsigned int	log_gauge_overflow;

	/* caches check for opcodes */
	guest_ptr	chk_addr;
	long		chk_opcode;

	bool		hit_syscall;

	MemLog		*mem_log;
	bool		xchk_eflags;
};

#endif
