#include <assert.h>
#include "cpu/ptimgarm.h"
#include "guestcpustate.h"

extern "C" {
#include "valgrind/libvex_guest_arm.h"
}

PTImgARM::PTImgARM(GuestPTImg* gs, int in_pid)
: PTImgArch(gs, in_pid)
{}

PTImgARM::~PTImgARM() {}

bool PTImgARM::isMatch(void) const { assert (0 == 1 && "STUB"); }
bool PTImgARM::doStep(guest_ptr start, guest_ptr end, bool& hit_syscall)
{ assert (0 == 1 && "STUB"); }

uintptr_t PTImgARM::getSysCallResult() const { assert (0 == 1 && "STUB"); }

const VexGuestARMState& PTImgARM::getVexState(void) const
{ return *((const VexGuestARMState*)gs->getCPUState()->getStateData()); }

bool PTImgARM::isRegMismatch() const { assert (0 == 1 && "STUB"); }
void PTImgARM::printFPRegs(std::ostream&) const { assert (0 == 1 && "STUB"); }
void PTImgARM::printUserRegs(std::ostream&) const { assert (0 == 1 && "STUB"); }
guest_ptr PTImgARM::getStackPtr() const { assert (0 == 1 && "STUB"); }
void PTImgARM::slurpRegisters() { assert (0 == 1 && "STUB"); }
void PTImgARM::stepSysCall(SyscallsMarshalled*) { assert (0 == 1 && "STUB"); }
long int PTImgARM::setBreakpoint(guest_ptr) { assert (0 == 1 && "STUB"); }
guest_ptr PTImgARM::undoBreakpoint() { assert (0 == 1 && "STUB"); }
guest_ptr PTImgARM::getPC() { assert (0 == 1 && "STUB"); }
bool PTImgARM::canFixup(
	const std::vector<std::pair<guest_ptr, unsigned char> >&,
	bool has_memlog) const { assert (0 == 1 && "STUB"); }
bool PTImgARM::breakpointSysCalls(guest_ptr, guest_ptr) { assert (0 == 1 && "STUB"); }
void PTImgARM::revokeRegs() { assert (0 == 1 && "STUB"); }
