#include "syscall/syscallparams.h"
#include "cpu/i386windowsabi.h"
#include "guestcpustate.h"
#include "guest.h"

I386WindowsABI::I386WindowsABI(Guest* g_)
: RegStrABI(g_, nullptr, "EAX", "EAX", true)
, edx_off(g_->getCPUState()->name2Off("EDX"))
{
	use_linux_sysenter = false;
}

SyscallParams I386WindowsABI::getSyscallParams(void) const
{
	auto	dat = (const uint8_t*)g->getCPUState()->getStateData();
	uint32_t edx_v = *((const uint32_t*)(dat + edx_off));
#define GET_ARGN(x,n)	g->getMem()->readNative(guest_ptr(edx_v), n)
	return SyscallParams(
		getSyscallResult(), /* sysnr */
		GET_ARGN(x86,0),
		GET_ARGN(x86,1),
		GET_ARGN(x86,2),
		GET_ARGN(x86,3),
		GET_ARGN(x86,4),
		GET_ARGN(x86,5));
}
