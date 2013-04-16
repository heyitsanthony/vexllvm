#ifndef PTIMGCHK_H
#define PTIMGCHK_H

#include <sys/types.h>
#include <sys/user.h>
#include "guestptimg.h"
#include "ptimgarch.h"

class MemLog;
class SyscallsMarshalled;

class PTImgChk : public GuestPTImg
{
public:
	PTImgChk(
		const char* binname,
		bool dummy = false /* so templates work */);
	virtual ~PTImgChk();

	guest_ptr stepToBreakpoint(void);
	void stepThroughBounds(guest_ptr start, guest_ptr end);
	void ignoreSysCall(void);
	void stepSysCall(SyscallsMarshalled* sc);
	guest_ptr continueForwardWithBounds(guest_ptr start, guest_ptr end);

	void printShadow(std::ostream& os) const;

	void stackTraceShadow(
		std::ostream& os,
		guest_ptr b = guest_ptr(0),
		guest_ptr e = guest_ptr(0));
	void printTraceStats(std::ostream& os);

	bool isMatch() const;

	bool fixup(const std::vector<InstExtent>& insts);
	bool breakpointSysCalls(guest_ptr ip_begin, guest_ptr ip_end);
	void resetBreakpoint(guest_ptr addr)
	{ GuestPTImg::resetBreakpoint(child_pid, addr); }

	void setBreakpoint(guest_ptr addr)
	{ GuestPTImg::setBreakpoint(child_pid, addr); }

	void pushRegisters(void);

	uintptr_t getSysCallResult() const;
	MemLog* getMemLog(void) { return mem_log; }

	pid_t getPID(void) const { return child_pid; }

protected:
	virtual void handleChild(pid_t pid);

private:
	bool isStackMatch(void) const;
	bool isMatchMemLog() const;

	void waitForSingleStep(void);

	void printMemory(std::ostream& os) const;

	void readMemLogData(char* data) const;

	pid_t		child_pid;

	uint64_t	bp_steps;
	uint64_t	blocks;

	bool		log_steps;

	bool		hit_syscall;

	MemLog		*mem_log;
	bool		xchk_stack;
};

#endif
