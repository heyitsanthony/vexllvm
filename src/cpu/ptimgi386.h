#ifndef PTIMGI386_H
#define PTIMGI386_H

#include <sys/user.h>
#include <set>
#include "ptimgarch.h"

class PTImgI386 : public PTImgArch
{
public:
	PTImgI386(GuestPTImg* gs, int in_pid);
	virtual ~PTImgI386();

	void setPID(int in_pid) override;

	bool doStep(guest_ptr start, guest_ptr end, bool& hit_syscall) override;
	void stepSysCall(SyscallsMarshalled*) override { assert (false && "STUB"); }

	void slurpRegisters() override;

	virtual void setFakeInfo(const char* info_file) override;
	virtual bool stepInitFixup(void) override;

protected:
	virtual bool patchCPUID(void) { return true; }
	virtual void trapCPUIDs(uint64_t bias) {}

	/* return PC iff on cpuid instruction, otherwise 0 */
	uint64_t checkCPUID(void);

	/* provided by VEXLLVM_FAKE_CPUID */
	std::set<uint64_t>		patch_offsets;
	/* overwritten instructions (addr, data) */
	std::map<uint64_t, uint64_t>	cpuid_insts;
};

#endif
