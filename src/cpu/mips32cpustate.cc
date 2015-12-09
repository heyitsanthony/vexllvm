#include <stdio.h>
#include "Sugar.h"
#include <stddef.h>
#include <iostream>
#include <cstring>
#include <vector>

#include "guest.h"
#include "mips32cpustate.h"
extern "C" {
#include <valgrind/libvex_guest_mips32.h>
}

#define state2mips32()	((VexGuestMIPS32State*)(state_data))

const char* MIPS32CPUState::abi_spim_scregs[] =
{ "R2", "R4", "R5", "R6", "R7", "R8", "R9", NULL};

static struct guest_ctx_field mips32_fields[] =
{
//	{32, 32, "GPR"},
	{32, 1, "R0"}, {32, 1, "R1"}, {32, 1, "R2"}, {32, 1, "R3"},
	{32, 1, "R4"}, {32, 1, "R5"}, {32, 1, "R6"}, {32, 1, "R7"},
	{32, 1, "R8"}, {32, 1, "R9"}, {32, 1, "R10"}, {32, 1, "R11"},
	{32, 1, "R12"}, {32, 1, "R13"}, {32, 1, "R14"}, {32, 1, "R15"},
	{32, 1, "R16"}, {32, 1, "R17"}, {32, 1, "R18"}, {32, 1, "R19"},
	{32, 1, "R20"}, {32, 1, "R21"}, {32, 1, "R22"}, {32, 1, "R23"},
	{32, 1, "R24"}, {32, 1, "R25"}, {32, 1, "R26"}, {32, 1, "R27"},
	{32, 1, "R28"}, {32, 1, "R29"}, {32, 1, "R30"}, {32, 1, "R31"},

	{32, 1, "PC"},
	{32, 1, "HI"},
	{32, 1, "LO"},
	{32, 32, "FPR"},

	{32, 1, "FIR"},
	{32, 1, "FCCR"},
	{32, 1, "FEXR"},
	{32, 1, "FENR"},
	{32, 1, "FCSR"},

	{32, 1, "ULR"},
	{32, 1, "EMWARN"},

	{32, 1, "CMSTART"},
	{32, 1, "CMLEN"},
	{32, 1, "NRADDR"},

	{32, 1, "FAILADDR"},
	{32, 1, "COUNTER"},
	{32, 1, "COND"},

	{32, 5, "pad"},
	/* END VEX STRUCTURE */

	{0}	/* time to stop */
};

MIPS32CPUState::MIPS32CPUState()
	: VexCPUState(mips32_fields)
{
	state_byte_c = sizeof(VexGuestMIPS32State);
	state_data = new uint8_t[state_byte_c+1];
	memset(state_data, 0, state_byte_c+1);
	exit_type = &state_data[state_byte_c];
}

MIPS32CPUState::~MIPS32CPUState() { delete [] state_data; }

void MIPS32CPUState::setPC(guest_ptr ip) { state2mips32()->guest_PC = ip; }

guest_ptr MIPS32CPUState::getPC(void) const
{ return guest_ptr(state2mips32()->guest_PC); }

void MIPS32CPUState::setStackPtr(guest_ptr stack_ptr)
{ state2mips32()->guest_r29 = (uint64_t)stack_ptr; }

guest_ptr MIPS32CPUState::getStackPtr(void) const
{ return guest_ptr(state2mips32()->guest_r29); }

void MIPS32CPUState::print(std::ostream& os, const void* regctx) const
{
	const VexGuestMIPS32State	*s;
	
	s = (const VexGuestMIPS32State*)regctx;
#define PRINT_REG(X) os << #X": " << (void*)(uintptr_t)s->guest_##X << "\n";

	for (int i = 0; i < 32; i++) {
		os
		<< "r" << i << ": "
		<< (void*)((long)(((const uint32_t*)&s->guest_r0))[i])
		<< '\n';
	}

	PRINT_REG(PC);
	PRINT_REG(HI);
	PRINT_REG(LO);

	for (int i = 0; i < 32; i++) {
		os
		<< "f" << i << ": "
		<< (void*)((long)(((const uint32_t*)&s->guest_f0)[i]))
		<< '\n';
	}

	PRINT_REG(FIR);
	PRINT_REG(FCCR);
	PRINT_REG(FEXR);
	PRINT_REG(FENR);
	PRINT_REG(FCSR);
}