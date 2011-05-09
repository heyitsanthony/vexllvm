#include <iostream>
#include <string.h>

#include "util.h"
#include "guestcpustate.h"
#include "gueststate.h"

using namespace llvm;

#define STACK_BYTES	(64*1024)

GuestState::GuestState(void)
{
	cpu_state = new GuestCPUState();
	stack = new uint8_t[STACK_BYTES];
	memset(stack, 0x30, STACK_BYTES);
	cpu_state->setStackPtr(stack + STACK_BYTES-256 /*redzone+gunk*/);
}

GuestState::~GuestState(void)
{
	delete cpu_state;
	delete [] stack;
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
