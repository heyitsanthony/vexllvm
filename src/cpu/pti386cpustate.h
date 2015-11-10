#ifndef PTI386CPUSTATE_H
#define PTI386CPUSTATE_H

#include "ptcpustate.h"

class PTI386CPUState : public PTCPUState
{
public:
	PTI386CPUState(pid_t in_pid);
	~PTI386CPUState();

	void ignoreSysCall(void) override { assert(false && "STUB"); }
	uint64_t dispatchSysCall(const SyscallParams& sp, int& wss) override {
		assert (false && "STUB");
	}
	void loadRegs(void) override;
	guest_ptr undoBreakpoint(void) override;
	long setBreakpoint(guest_ptr addr) override;

	guest_ptr getPC(void) const override { assert (false && "STUB"); }
	guest_ptr getStackPtr(void) const override;
	uintptr_t getSysCallResult() const override { assert (false && "STUB"); }

	void setStackPtr(guest_ptr) override { assert (false && "STUB"); }
	void setPC(guest_ptr) override { assert (false && "STUB"); }

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
