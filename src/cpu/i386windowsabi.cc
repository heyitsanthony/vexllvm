#include "syscall/syscallparams.h"
#include "cpu/i386windowsabi.h"
#include "guestcpustate.h"
#include "guest.h"

extern "C" {
#include <valgrind/libvex_guest_x86.h>
}

#define state2i386()	((VexGuestX86State*)(g->getCPUState()->getStateData()))

SyscallParams I386WindowsABI::getSyscallParams(void) const
{
	const VexGuestX86State	*x86(state2i386());
#define GET_ARGN(x,n)	g->getMem()->readNative(guest_ptr(x->guest_EDX), n)
	return SyscallParams(
		x86->guest_EAX, /* sysnr */
		GET_ARGN(x86,0),
		GET_ARGN(x86,1),
		GET_ARGN(x86,2),
		GET_ARGN(x86,3),
		GET_ARGN(x86,4),
		GET_ARGN(x86,5));
}

void I386WindowsABI::setSyscallResult(uint64_t ret)
{ state2i386()->guest_EAX = (uint32_t)ret; }

uint64_t I386WindowsABI::getExitCode(void) const
{ return state2i386()->guest_EAX; }

uint64_t I386WindowsABI::getSyscallResult(void) const
{ return state2i386()->guest_EAX; }
