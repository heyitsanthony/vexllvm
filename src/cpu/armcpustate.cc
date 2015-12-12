#include <stdio.h>
#include <stddef.h>
#include "Sugar.h"

#include <iostream>
#include <cstring>
#include <vector>

#include "sc_xlate.h"
#include "armcpustate.h"
extern "C" {
#include <valgrind/libvex_guest_arm.h>
}

struct ExtVexGuestARMState
{
	VexGuestARMState	guest_vex;
	unsigned int		guest_LINKED;
};
#define state2arm()	((VexGuestARMState*)(state_data))
#define state2arm_ext() ((ExtVexGuestARMState*)(state_data))

#define reg_field_ent_w_n(x, w, n) { #x, w, n, offsetof(VexGuestARMState, guest_##x), true }
#define reg_field_ent(x) reg_field_ent_w_n(x, sizeof(((VexGuestARMState*)0)->guest_##x), 1)
#define raw_field_ent_w_n(x, w, n) { #x, w, n, offsetof(VexGuestARMState, x), true }
#define raw_field_ent(x) raw_field_ent_w_n(x, sizeof((VexGuestARMState*)0)->x, 1)

/* ripped from libvex_guest_arm */
static struct guest_ctx_field arm_fields[] =
{
	raw_field_ent(host_EvC_FAILADDR),
	raw_field_ent(host_EvC_COUNTER),
	{"R", 4, 16, offsetof(VexGuestARMState, guest_R0), true},

	reg_field_ent(CC_OP),
	reg_field_ent(CC_DEP1),
	reg_field_ent(CC_DEP2),
	reg_field_ent(CC_NDEP),
	reg_field_ent(QFLAG32),
	reg_field_ent(GEFLAG0),
	reg_field_ent(GEFLAG1),
	reg_field_ent(GEFLAG2),
	reg_field_ent(GEFLAG3),
	reg_field_ent(EMNOTE),
	reg_field_ent(CMSTART),
	reg_field_ent(CMLEN),

	/* unredirected guest addr at start of translation whose
	 * start has been redirected */
	reg_field_ent(NRADDR),
	reg_field_ent(IP_AT_SYSCALL),


	{"D", 8, 32, offsetof(VexGuestARMState, guest_D0), true},
	reg_field_ent(FPSCR),

	reg_field_ent(TPIDRURO),

	reg_field_ent(ITSTATE),
	raw_field_ent(padding1),

	/* END VEX STRUCTURE */
	/* we set this when load linked happens
	   clear it when a later store conditional
	   should fail */
	{ "LINKED", 4, 1, sizeof(VexGuestARMState), true},
	{0}	/* time to stop */
};

ARMCPUState::ARMCPUState(void)
	: VexCPUState(arm_fields)
{
	state_byte_c = sizeof(ExtVexGuestARMState);
	state_data = new uint8_t[state_byte_c+1];
	memset(state_data, 0, state_byte_c+1);
	exit_type = &state_data[state_byte_c];
	xlate = std::make_unique<ARMSyscallXlate>();
}

ARMCPUState::~ARMCPUState()
{
	delete [] state_data;
}

void ARMCPUState::setPC(guest_ptr ip) { state2arm()->guest_R15T = ip; }

guest_ptr ARMCPUState::getPC(void) const {
	/* todo is this right, do we need to mask the low
	   bits that control whether its in thumb mode or not? */
	return guest_ptr(state2arm()->guest_R15T);
}

void ARMCPUState::setStackPtr(guest_ptr stack_ptr)
{ state2arm()->guest_R13 = stack_ptr; }

guest_ptr ARMCPUState::getStackPtr(void) const
{ return guest_ptr(state2arm()->guest_R13); }


void ARMCPUState::setThreadPointer(uint32_t v)
{ state2arm()->guest_TPIDRURO = v; }

void ARMCPUState::print(std::ostream& os, const void* regctx) const
{
	const ExtVexGuestARMState*	s;
	const uint32_t			*gpr_base;

	s = (const ExtVexGuestARMState*)regctx;
	gpr_base = &s->guest_vex.guest_R0;
	for (int i = 0; i < 16; i++) {
		os	<< "R" << i << ": "
			<< (void*)((long)gpr_base[i]) << std::endl;
	}

	const unsigned long long *vpr_base = &s->guest_vex.guest_D0;
	for (int i = 0; i < 16; i++) {
		os
		<< "D" << i << ": "
		<< (void*)vpr_base[i] << std::endl;
	}
	os << "FPCSR: "  << (void*)((long)s->guest_vex.guest_FPSCR) << "\n";
	/* tls */
	os << "TPIDRURO: "<< (void*)((long)s->guest_vex.guest_TPIDRURO)<<"\n";
	os << "LINKED: "  << (void*)((long)s->guest_LINKED) << "\n";
}

#ifdef __arm__
void ARMCPUState::setRegs(
	const user_regs* regs, const uint8_t* vfpregs,
	void* thread_area)
{
	memcpy(&state2arm()->guest_R0, regs, 16*4);
	memcpy(&state2arm()->guest_D0, vfpregs, 32*8/*fpreg*/+4/*fpscr*/);

	state2arm()->guest_TPIDRURO = (long)thread_area;
	//TODO: some kind of flags, checking but i don't yet understand this
	//mess of broken apart state.
}
#endif

unsigned int ARMCPUState::getStackRegOff(void) const
{
	return offsetof(VexGuestARMState, guest_R13);
}

unsigned int ARMCPUState::getRetOff(void) const
{
	return offsetof(VexGuestARMState, guest_R0);
}

