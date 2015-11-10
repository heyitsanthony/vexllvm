#ifndef PTIMGAMD64_H
#define PTIMGAMD64_H

#include <sys/user.h>
#include "ptimgarch.h"

class PTImgAMD64 : public PTImgArch
{
public:
	PTImgAMD64(GuestPTImg* gs, int in_pid);
	virtual ~PTImgAMD64(void) {}

	void setPID(int in_pid) override;
	bool doStep(guest_ptr start, guest_ptr end, bool& hit_syscall) override;

	void stepSysCall(SyscallsMarshalled* sc_m) override;
	void ignoreSysCall(void) override;
	void fixupRegsPreSyscall(void) override;

	void slurpRegisters(void) override;

	void getRegs(user_regs_struct&) const;
	void setRegs(const user_regs_struct&);

protected:
	bool doStepPreCheck(guest_ptr start, guest_ptr end, bool& hit_syscall);
	bool isPushF(void);
	bool isOnCPUID(void);
	bool isOnRDTSC(void);
	bool isOnSysCall(void);
};

#endif
