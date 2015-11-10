#ifndef PTIMGARCH_H
#define PTIMGARCH_H

#include "guestptimg.h"
#include "ptshadow.h"

class PTCPUState;
class SyscallsMarshalled;

class PTImgArch
{
public:
	PTImgArch(GuestPTImg* in_gs, int in_pid);
	virtual ~PTImgArch();

	int getPID(void) const { return child_pid; }
	virtual void setPID(int in_pid) = 0;

	virtual bool doStep(
		guest_ptr start, guest_ptr end, bool& hit_syscall) = 0;

	virtual void stepSysCall(SyscallsMarshalled* sc_m) = 0;
	virtual bool filterSysCall(void) { return false; }
	virtual void ignoreSysCall(void) { assert (0 == 1 && "STUB"); }
	virtual void fixupRegsPreSyscall(void)
	{ assert (0 == 1 && "Not implemented for this arch"); }

	guest_ptr stepToBreakpoint(void);
	void stepThroughBounds(guest_ptr start, guest_ptr end);
	guest_ptr continueForwardWithBounds(guest_ptr start, guest_ptr end);

	virtual void slurpRegisters(void) = 0;

	bool breakpointSysCalls(
		const guest_ptr ip_begin,
		const guest_ptr ip_end);

	uint64_t getSteps(void) const { return steps; }
	void incBlocks(void) { blocks++; }

	virtual void setFakeInfo(const char* info_file)
	{ assert (0 == 1 && "Not implemented for this arch"); }

	virtual bool stepInitFixup(void)
	{ assert (0 == 1 && "Not implemented for this arch");
	  return false; }

	/* starts running guest natively by jumping into context */
	virtual void restore(void) { assert (0 == 1 && "Not implemented"); }

	uint64_t dispatchSysCall(const SyscallParams& sp);

	PTCPUState& getPTCPU(void) { return *pt_cpu; }

	int getWaitStatus(void) const { return wss_status; }
	bool isSigSegv(void) const;

protected:
	void waitForSingleStep(void);
	virtual void handleBadProgress(void) { abort(); }
	void checkWSS(void);

	long getInsOp(void) const;
	long getInsOp(long pc) const;


	GuestPTImg	*gs;
	std::unique_ptr<PTCPUState>	pt_cpu;
	int		child_pid;
	bool		log_steps;

	int		wss_status;

private:
	uint64_t	steps;
	uint64_t	bp_steps;
	uint64_t	blocks;
	bool		hit_syscall;
	unsigned int	log_gauge_overflow;

	/* caches check for opcodes */
	mutable guest_ptr	chk_addr;
	mutable long		chk_opcode;
};

#endif
