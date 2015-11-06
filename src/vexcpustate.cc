#include "cpu/amd64cpustate.h"
#include "cpu/armcpustate.h"
#include "cpu/i386cpustate.h"
#include "cpu/mips32cpustate.h"

#include "vexcpustate.h"

VexCPUState* VexCPUState::create(Arch::Arch arch)
{
	switch(arch) {
	case Arch::X86_64:	return new AMD64CPUState();
	case Arch::I386:	return new I386CPUState();
	case Arch::ARM:		return new ARMCPUState();
	case Arch::MIPS32:	return new MIPS32CPUState();
	default:
		assert(!"supported guest architecture");
	}
}

void VexCPUState::registerCPUs(void)
{
	GuestCPUState::registerCPU(
		Arch::X86_64,
		[] { return new AMD64CPUState(); });
	GuestCPUState::registerCPU(
		Arch::I386,
		[] { return new I386CPUState(); });
	GuestCPUState::registerCPU(
		Arch::ARM,
		[] { return new ARMCPUState(); });
	GuestCPUState::registerCPU(
		Arch::MIPS32,
		[] { return new MIPS32CPUState(); });
}
