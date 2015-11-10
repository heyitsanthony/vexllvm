#ifndef PTAMD64CPUSTATE_H
#define PTAMD64CPUSTATE_H

#include "ptcpustate.h"

class PTAMD64CPUState : public PTCPUState
{
public:
	PTAMD64CPUState(pid_t in_pid);
	~PTAMD64CPUState();

	void ignoreSysCall(void) override;
	uint64_t dispatchSysCall(const SyscallParams& sp, int& wss) override;
	void loadRegs(void) override;
	guest_ptr undoBreakpoint(void) override;
	long setBreakpoint(guest_ptr addr) override;

	guest_ptr getPC(void) const override;
	guest_ptr getStackPtr(void) const override;
	uintptr_t getSysCallResult() const override;

	void setStackPtr(guest_ptr) override;
	void setPC(guest_ptr) override;

	bool isSyscallOp(guest_ptr addr, long v) const override;

	void print(std::ostream& os, const void*) const override {
		assert(false && "STUB");
		abort();
	}

	const char* off2Name(unsigned int off) const override {
		assert(false && "STUB");
		abort();
		return nullptr;
	}

	const struct guest_ctx_field* getFields(void) const override {
		assert (false && "STUB");
		abort();
		return nullptr;
	}

	// used by ptimgarch
	struct user_regs_struct& getRegs(void) const;
	struct user_fpregs_struct& getFPRegs(void) const;
	void setRegs(const user_regs_struct& regs);

private:
	void reloadRegs(void) const;
};

#endif
