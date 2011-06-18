#include <stdlib.h>
#include <iostream>
#include <string.h>

#include "util.h"
#include "guestcpustate.h"
#include "guest.h"

using namespace llvm;

Guest::Guest(const char* in_bin_path)
: mem(NULL)
{
	bin_path = strdup(in_bin_path);
}

Guest::~Guest(void)
{
	if (mem) {
		delete mem;
		mem = NULL;
	}
	free(bin_path);
	delete cpu_state;
}

SyscallParams Guest::getSyscallParams(void) const
{
	return cpu_state->getSyscallParams();
}

void Guest::setSyscallResult(uint64_t ret)
{
	cpu_state->setSyscallResult(ret);
}

std::string Guest::getName(void* x) const { 
	return hex_to_string((uintptr_t)x); 
}

uint64_t Guest::getExitCode(void) const
{
	return cpu_state->getExitCode();
}

void Guest::print(std::ostream& os) const { cpu_state->print(os); }

std::list<GuestMem::Mapping> Guest::getMemoryMap(void) const
{
	assert (mem != NULL);
	return mem->getMaps();
}
