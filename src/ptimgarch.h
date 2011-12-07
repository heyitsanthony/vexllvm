#ifndef PTIMGARCH_H
#define PTIMGARCH_H

#include "guestptimg.h"

class SyscallsMarshalled;

class PTImgArch
{
public:
	virtual bool isRegMismatch(void) const = 0;
	virtual void printFPRegs(std::ostream& os) const = 0;
	virtual void printUserRegs(std::ostream& os) const = 0;
	virtual guest_ptr getStackPtr(void) const = 0;
	virtual void slurpRegisters(void) = 0;
	virtual void stepSysCall(SyscallsMarshalled* sc_m) = 0;

	virtual long setBreakpoint(guest_ptr addr) = 0;
	virtual void resetBreakpoint(guest_ptr addr, long v) = 0;
	virtual guest_ptr undoBreakpoint() = 0;
	virtual bool doStep(
		guest_ptr start, guest_ptr end, bool& hit_syscall) = 0;

	virtual guest_ptr getPC(void) = 0;

	virtual bool canFixup(
		const std::vector<InstExtent>& insts,
		bool has_memlog) const = 0;

	virtual bool isMatch(void) const = 0;
	virtual uintptr_t getSysCallResult() const = 0;

	virtual bool breakpointSysCalls(
		const guest_ptr ip_begin,
		const guest_ptr ip_end) = 0;

	virtual ~PTImgArch();

	uint64_t getSteps(void) const { return steps; }
	void incBlocks(void) { blocks++; }
	virtual void revokeRegs(void) = 0;

protected:
	PTImgArch(GuestPTImg* in_gs, int in_pid);
	virtual void waitForSingleStep(void);
	void copyIn(guest_ptr dst, const void* src, unsigned int bytes) const;


	GuestPTImg	*gs;
	int		child_pid;
	bool		log_steps;

private:
	uint64_t	steps;
	uint64_t	blocks;
	unsigned int	log_gauge_overflow;
};

#endif
