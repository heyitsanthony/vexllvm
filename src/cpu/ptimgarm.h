#include "cpu/ptimgarm.h"

extern "C" {
#include "valgrind/libvex_guest_arm.h"
}

bool PTImgARM::isMatch(void) const { assert (0 == 1 && "STUB"); }
bool PTImgARM::doStep(guest_ptr start, guest_ptr end, bool& hit_syscall)
{ assert (0 == 1 && "STUB"); }

uintptr_t getSysCallResult() const
{ assert (0 == 1 && "STUB"); }

const VexGuestARMState& getVexState(void) const
{
	return *((const VexGuestARMState*)gs->getCPUState()->getStateData());
}

