#include <assert.h>
#include "ptshadow.h"

std::map<Arch::Arch, make_ptshadow_t> PTShadow::makers;

void PTShadow::registerCPU(Arch::Arch a, make_ptshadow_t pts)
{
	assert(makers.count(a) == 0);
	makers[a] = pts;
}

PTShadow* PTShadow::create(Arch::Arch arch, GuestPTImg* gpi, int pid)
{
	if (!makers.count(arch)) return nullptr;
	return makers[arch](gpi, pid);
}
