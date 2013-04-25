#ifndef PTIMGI386_H
#define PTIMGI386_H

#include <sys/user.h>
#include <set>
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
	GuestPTImg::FixupDir canFixup(
		const std::vector<std::pair<guest_ptr, unsigned char> >&,
		bool has_memlog) const;
	bool breakpointSysCalls(guest_ptr, guest_ptr);
	void revokeRegs();
	void resetBreakpoint(guest_ptr addr, long v);

	virtual void setFakeInfo(const char* info_file);
	virtual void stepInitFixup(void);
private:
	/* return PC iff on cpuid instruction, otherwise 0 */
	uint64_t checkCPUID(void);

	/* return true iff on trapped cpuid instruction */
	bool patchCPUID(void);

	void setupGDT(void);
	bool readThreadEntry(unsigned idx, VEXSEG* buf);
	const VexGuestX86State& getVexState(void) const;
	char	shadow_user_regs[17*4];

	/* provided by VEXLLVM_FAKE_CPUID */
	std::set<uint64_t>	patch_offsets;
	/* overwritten instructions (addr, data) */
	std::map<uint64_t, uint64_t>	cpuid_insts;
};

#endif
