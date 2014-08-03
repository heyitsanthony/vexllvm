#include "syscallnamer.h"
#include "syscallparams.h"
#include "../guest.h"
#include "xlate-linux-x86.h"
#include "xlate-linux-x64.h"


const char* SyscallNamer::xlate(const Guest* g, const SyscallParams& sp)
{ return xlate(g, sp.getSyscall()); }

const char* SyscallNamer::xlate(const Guest* g, unsigned int sysnr)
{
	const char *ret = NULL;

	switch(g->getArch()) {
	case Arch::I386:
		if (sizeof(xlate_tab_x86)/sizeof(char*) > sysnr)
			ret = xlate_tab_x86[sysnr];
		break;
	case Arch::X86_64:
		if (sizeof(xlate_tab_x64)/sizeof(char*) > sysnr)
			ret = xlate_tab_x64[sysnr];
		break;
	default: break;
	}

	if (ret == NULL) ret = "???";

	return ret;
}
