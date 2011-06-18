#include "guestcpustate.h"
#include "amd64cpustate.h"

GuestCPUState* GuestCPUState::create(Arch::Arch arch) {
	switch(arch) {
	case Arch::X86_64:
		return new AMD64CPUState();
	default:
		assert(!"supported guest architecture");
	}
}
