#ifndef PTIMGARCH_H
#define PTIMGARCH_H

#include "guestptimg.h"

class SyscallsMarshalled;
class PTCPUState;

class PTImgArch
{
public:
	virtual bool isRegMismatch(void) const = 0;
	virtual void printFPRegs(std::ostream& os) const = 0;
	virtual void printUserRegs(std::ostream& os) const = 0;
	virtual void slurpRegisters(void) = 0;
	virtual void pushRegisters(void) { assert (0 == 1 && "STUB"); }
	virtual void stepSysCall(SyscallsMarshalled* sc_m) = 0;
	virtual void ignoreSysCall(void) { assert (0 == 1 && "STUB"); }

	int getPID(void) const { return child_pid; }
	virtual void setPID(int in_pid) = 0;

	virtual bool doStep(
		guest_ptr start, guest_ptr end, bool& hit_syscall) = 0;

	virtual GuestPTImg::FixupDir canFixup(
		const std::vector<InstExtent>& insts,
		bool has_memlog) const = 0;

	virtual void fixupRegsPreSyscall(void)
	{ assert (0 == 1 && "Not implemented for this arch"); }

	virtual bool isMatch(void) const = 0;

	virtual bool breakpointSysCalls(
		const guest_ptr ip_begin,
		const guest_ptr ip_end) = 0;

	virtual ~PTImgArch();

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
	PTImgArch(GuestPTImg* in_gs, int in_pid);
	void waitForSingleStep(void);
	void pushBadProgress(void);
	void checkWSS(void);

	GuestPTImg	*gs;
	std::unique_ptr<PTCPUState>	pt_cpu;
	int		child_pid;
	bool		log_steps;

	int		wss_status;
private:
	uint64_t	steps;
	uint64_t	blocks;
	unsigned int	log_gauge_overflow;
};

#endif
