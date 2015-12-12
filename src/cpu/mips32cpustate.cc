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

#define reg_field_ent_w_n(x, w, n) { #x, w, n, offsetof(VexGuestMIPS32State, guest_##x), true }
#define reg_field_ent(x) reg_field_ent_w_n(x, sizeof(((VexGuestMIPS32State*)0)->guest_##x), 1)
#define raw_field_ent_w_n(x, w, n) { #x, w, n, offsetof(VexGuestMIPS32State, x), true }
#define raw_field_ent(x) raw_field_ent_w_n(x, sizeof((VexGuestMIPS32State*)0)->x, 1)


static struct guest_ctx_field mips32_fields[] =
{
	{ "R", 4, 32, offsetof(VexGuestMIPS32State, guest_r0), true },
	reg_field_ent(PC),
	reg_field_ent(HI),
	reg_field_ent(LO),
	{ "F", 8, 32, offsetof(VexGuestMIPS32State, guest_f0), true },

	reg_field_ent(FIR),
	reg_field_ent(FCCR),
	reg_field_ent(FEXR),
	reg_field_ent(FENR),
	reg_field_ent(FCSR),

	reg_field_ent(ULR),
	reg_field_ent(EMNOTE),

	reg_field_ent(CMSTART),
	reg_field_ent(CMLEN),
	reg_field_ent(NRADDR),

	raw_field_ent(host_EvC_FAILADDR),
	raw_field_ent(host_EvC_COUNTER),
	reg_field_ent(COND),

	raw_field_ent(padding),
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