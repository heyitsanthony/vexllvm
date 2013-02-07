#ifndef PTIMGI386_H
#define PTIMGI386_H

#include <sys/user.h>
#include "ptimgarch.h"

extern "C" {
#include <valgrind/libvex_guest_x86.h>
}

#define VEXSEG	VexGuestX86SegDescr

class PTImgI386 : public PTImgArch
{
public:
	PTImgI386(GuestPTImg* gs, int in_pid);
	virtual ~PTImgI386();

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
	bool canFixup(
		const std::vector<std::pair<guest_ptr, unsigned char> >&,
		bool has_memlog) const;
	bool breakpointSysCalls(guest_ptr, guest_ptr);
	void revokeRegs();
	void resetBreakpoint(guest_ptr addr, long v);

	virtual void setFakeInfo(const char* info_file);
	virtual uint64_t stepInitFixup(void);
private:
	void setupGDT(void);
	bool readThreadEntry(unsigned idx, VEXSEG* buf);
	const VexGuestX86State& getVexState(void) const;
	char	shadow_user_regs[17*4];
};

#endif
