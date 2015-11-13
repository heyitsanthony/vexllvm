#include "guest.h"
#include "guestabi.h"
#include "cpu/linux_abi.h"

const char* LinuxABI::scregs_i386[] =
{
	"EAX", "EBX", "ECX", "EDX", "ESI", "EDI", "EBP", nullptr
};
#define	ABI_LINUX_I386	scregs_i386,	\
	"EAX",	/* syscall result */	\
	"EBX",	/* get exit code */	\
	true	/* 32-bit */

const char* LinuxABI::scregs_amd64[] =
{
	"RAX", "RDI", "RSI", "RDX", "R10", "R8", "R9", nullptr
};
#define	ABI_LINUX_AMD64	scregs_amd64, "RAX", "RDI", false

/* its possible that the actual instruction can encode
   something in the non-eabi case, but we are restricting
   ourselves to eabi for now */
const char* LinuxABI::scregs_arm[] =
{
	"R7", "R0", "R1", "R2", "R3", "R4", "R5", nullptr 
};
#define	ABI_LINUX_ARM scregs_arm, "R0", "R0", true

GuestABI* LinuxABI::create(Guest& g)
{
	switch (g.getArch()) {
	case Arch::X86_64: return new RegStrABI(g, ABI_LINUX_AMD64);
	case Arch::ARM: return new RegStrABI(g, ABI_LINUX_ARM);
	case Arch::I386: return new RegStrABI(g, ABI_LINUX_I386);
	default:
		std::cerr << "Unknown guest arch for Linux ABI?\n";
		break;
	}
	return nullptr;
}
