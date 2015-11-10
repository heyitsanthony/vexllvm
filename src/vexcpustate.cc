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

#if defined(__amd64__)
#include "cpu/ptshadowamd64.h"
#include "cpu/ptshadowi386.h"
#endif

#ifdef __arm__
#include "cpu/ptshadowarm.h"
#endif

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

#if defined(__amd64__)
	PTShadow::registerCPU(
		Arch::I386,
		[] (GuestPTImg* gpi, int pid) {
			return new PTShadowI386(gpi, pid);
		}
	);
	PTShadow::registerCPU(
		Arch::X86_64,
		[] (GuestPTImg* gpi, int pid) {
			return new PTShadowAMD64(gpi, pid);
		}
	);
#elif defined(__arm__)
//	pt_arch = new PTImgARM(gpi, pid);
#endif

}
