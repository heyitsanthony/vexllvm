#include <stdlib.h>
#include <iostream>
#include <string.h>

#include "util.h"
#include "guestcpustate.h"
#include "guest.h"

using namespace llvm;

Guest::Guest(const char* in_bin_path)
{
	cpu_state = new GuestCPUState();
	bin_path = strdup(in_bin_path);
}

Guest::~Guest(void)
{
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

std::string Guest::getName(guestptr_t x) const { return hex_to_string(x); }

uint64_t Guest::getExitCode(void) const
{
	return cpu_state->getExitCode();
}

void Guest::print(std::ostream& os) const { cpu_state->print(os); }
