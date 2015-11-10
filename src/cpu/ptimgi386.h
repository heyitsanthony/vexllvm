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

	void setPID(int in_pid) override;
	bool isMatch(void) const override;
	bool doStep(guest_ptr start, guest_ptr end, bool& hit_syscall) override;

	bool isRegMismatch() const override;
	void printFPRegs(std::ostream&) const override;
	void printUserRegs(std::ostream&) const override;
	void slurpRegisters() override;
	void stepSysCall(SyscallsMarshalled*) override { assert (false && "STUB"); }
	GuestPTImg::FixupDir canFixup(
		const std::vector<std::pair<guest_ptr, unsigned char> >&,
		bool has_memlog) const override;

	virtual void setFakeInfo(const char* info_file) override;
	virtual bool stepInitFixup(void) override;

private:
	/* return PC iff on cpuid instruction, otherwise 0 */
	uint64_t checkCPUID(void);

	/* return true iff on trapped cpuid instruction */
	bool patchCPUID(void);

	void setupGDT(void);
	bool readThreadEntry(unsigned idx, VEXSEG* buf);
	const VexGuestX86State& getVexState(void) const;

	/* provided by VEXLLVM_FAKE_CPUID */
	std::set<uint64_t>	patch_offsets;
	/* overwritten instructions (addr, data) */
	std::map<uint64_t, uint64_t>	cpuid_insts;
};

#endif
