#ifndef PTSHADOWI386_H
#define PTSHADOWI386_H

#include "cpu/ptimgi386.h"
#include "ptshadow.h"

extern "C" {
#include <valgrind/libvex_guest_x86.h>
}

#define VEXSEG	VexGuestX86SegDescr

class PTShadowI386 : public PTImgI386, public PTShadow
{
public:
	PTShadowI386(GuestPTImg* gs, int in_pid);

	// shadow stuff
	void printUserRegs(std::ostream& os) const override;
	void printFPRegs(std::ostream& os) const override;
	void slurpRegisters(void) override;
	void pushRegisters(void) override { assert (false && "STUB"); }

	FixupDir canFixup(
		const std::vector<InstExtent>& insts,
		bool has_memlog) const override { assert (false && "STUB"); }

	bool isRegMismatch(void) const override;
	bool isMatch(void) const override;

protected:
	/* return true iff on trapped cpuid instruction */
	bool patchCPUID(void) override;
	void trapCPUIDs(uint64_t bias) override;

private:
	const VexGuestX86State& getVexState(void) const;
	VexGuestX86State& getVexState(void);

	void setupGDT(void);
	bool readThreadEntry(unsigned idx, VEXSEG* buf);
};
#endif
