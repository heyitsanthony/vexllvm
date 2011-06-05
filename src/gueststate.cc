#include <iostream>
#include <string.h>

#include "util.h"
#include "guestcpustate.h"
#include "gueststate.h"

using namespace llvm;

GuestState::GuestState(void)
{
	cpu_state = new GuestCPUState();
}

GuestState::~GuestState(void)
{
	delete cpu_state;
}

SyscallParams GuestState::getSyscallParams(void) const
{
	return cpu_state->getSyscallParams();
}

void GuestState::setSyscallResult(uint64_t ret)
{
	cpu_state->setSyscallResult(ret);
}

std::string GuestState::getName(guestptr_t x) const { return hex_to_string(x); }

uint64_t GuestState::getExitCode(void) const
{
	return cpu_state->getExitCode();
}

void GuestState::print(std::ostream& os) const { cpu_state->print(os); }
