#include <stdio.h>
#include <stddef.h>
#include "Sugar.h"

#include <iostream>
#include <cstring>
#include <vector>

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


const char* ARMCPUState::abi_linux_scregs[] =
{ "R7", "R0", "R1", "R2", "R3", "R4", "R5", NULL };

ARMCPUState::ARMCPUState(void)
{
	state_byte_c = getFieldsSize(getFields());
	state_data = new uint8_t[state_byte_c+1];
	memset(state_data, 0, state_byte_c+1);
	exit_type = &state_data[state_byte_c];
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

/* ripped from libvex_guest_arm */
static struct guest_ctx_field arm_fields[] =
{
	{32, 2, "EvC"},
//	{32, 16, "GPR"},

	{32, 1, "R0"},
	{32, 1, "R1"},
	{32, 1, "R2"},
	{32, 1, "R3"},
	{32, 1, "R4"},

	{32, 1, "R5"},
	{32, 1, "R6"},
	{32, 1, "R7"},
	{32, 1, "R8"},
	{32, 1, "R9"},

	{32, 1, "R10"},
	{32, 1, "R11"},
	{32, 1, "R12"},
	{32, 1, "R13"},
	{32, 1, "R14"},

	{32, 1, "R15T"},

	{32, 1, "CC_OP"},
	{32, 1, "CC_DEP1"},
	{32, 1, "CC_DEP2"},
	{32, 1, "CC_NDEP"},

	{32, 1, "QFLAG32"},

	{32, 1, "GEFLAG0"},
	{32, 1, "GEFLAG1"},
	{32, 1, "GEFLAG2"},
	{32, 1, "GEFLAG3"},
	
	{32, 1, "EMNOTE"},
	
	{32, 1, "CMSTART"},
	{32, 1, "CMLEN"},

	/* unredirected guest addr at start of translation whose
	 * start has been redirected */
	{32, 1, "NRADDR"},

	{32, 1, "IP_AT_SYSCALL"},

	{64, 32, "VFP_R"},
	{32, 1, "FPSCR"},

	{32, 1, "TPIDRURO"},

	{32, 1, "ITSTATE"},

	{32, 5, "pad"},

	/* END VEX STRUCTURE */
	{32, 1, "LINKED"},	/* we set this when load linked happens
				   clear it when a later store conditional
				   should fail */

	{0}	/* time to stop */
};

const struct guest_ctx_field* ARMCPUState::getFields(void) const
{ return arm_fields; }

const char* ARMCPUState::off2Name(unsigned int off) const
{
	switch (off) {
#define CASE_OFF2NAME2(x,y)				\
	CASE_OFF2NAME_4(VexGuestARMState, guest_##y)	\
	return #x;

#define CASE_OFF2NAME(x)	\
	CASE_OFF2NAME_4(VexGuestARMState, guest_##x)	\
	return #x;

#define CASE_OFF2NAME_NUM(x,y)	\
	CASE_OFF2NAME_4(VexGuestARMState, guest_##x##y)	\
	return #x"["#y"]";

	CASE_OFF2NAME_NUM(R,0)
	CASE_OFF2NAME_NUM(R,1)
	CASE_OFF2NAME_NUM(R,2)
	CASE_OFF2NAME_NUM(R,3)
	CASE_OFF2NAME_NUM(R,4)
	CASE_OFF2NAME_NUM(R,5)
	CASE_OFF2NAME_NUM(R,6)
	CASE_OFF2NAME_NUM(R,7)
	CASE_OFF2NAME_NUM(R,8)
	CASE_OFF2NAME_NUM(R,9)
	CASE_OFF2NAME_NUM(R,10)
	CASE_OFF2NAME2(FP, R11)
	CASE_OFF2NAME2(IP, R12)
	CASE_OFF2NAME2(SP, R13)
	CASE_OFF2NAME2(LR, R14)
	CASE_OFF2NAME2(PC, R15T)
	CASE_OFF2NAME(CC_OP)
	CASE_OFF2NAME(CC_DEP1)
	CASE_OFF2NAME(CC_DEP2)
	CASE_OFF2NAME(CC_NDEP)
	CASE_OFF2NAME(QFLAG32)
	CASE_OFF2NAME(GEFLAG0)
	CASE_OFF2NAME(GEFLAG1)
	CASE_OFF2NAME(GEFLAG2)
	CASE_OFF2NAME(GEFLAG3)
	CASE_OFF2NAME(EMNOTE)
	CASE_OFF2NAME(CMSTART)
	CASE_OFF2NAME(CMLEN)
	CASE_OFF2NAME(NRADDR)
	CASE_OFF2NAME(IP_AT_SYSCALL)
	CASE_OFF2NAME_NUM(D,0)
	CASE_OFF2NAME_NUM(D,1)
	CASE_OFF2NAME_NUM(D,2)
	CASE_OFF2NAME_NUM(D,3)
	CASE_OFF2NAME_NUM(D,4)
	CASE_OFF2NAME_NUM(D,5)
	CASE_OFF2NAME_NUM(D,6)
	CASE_OFF2NAME_NUM(D,7)
	CASE_OFF2NAME_NUM(D,8)
	CASE_OFF2NAME_NUM(D,9)
	CASE_OFF2NAME_NUM(D,10)
	CASE_OFF2NAME_NUM(D,11)
	CASE_OFF2NAME_NUM(D,12)
	CASE_OFF2NAME_NUM(D,13)
	CASE_OFF2NAME_NUM(D,14)
	CASE_OFF2NAME_NUM(D,15)
	CASE_OFF2NAME_NUM(D,16)
	CASE_OFF2NAME_NUM(D,17)
	CASE_OFF2NAME_NUM(D,18)
	CASE_OFF2NAME_NUM(D,19)
	CASE_OFF2NAME_NUM(D,20)
	CASE_OFF2NAME_NUM(D,21)
	CASE_OFF2NAME_NUM(D,22)
	CASE_OFF2NAME_NUM(D,23)
	CASE_OFF2NAME_NUM(D,24)
	CASE_OFF2NAME_NUM(D,25)
	CASE_OFF2NAME_NUM(D,26)
	CASE_OFF2NAME_NUM(D,27)
	CASE_OFF2NAME_NUM(D,28)
	CASE_OFF2NAME_NUM(D,29)
	CASE_OFF2NAME_NUM(D,30)
	CASE_OFF2NAME_NUM(D,31)
	CASE_OFF2NAME(FPSCR)
	CASE_OFF2NAME(TPIDRURO)
	CASE_OFF2NAME(ITSTATE)
#undef CASE_OFF2NAME2
#undef CASE_OFF2NAME
#define CASE_OFF2NAME2(x,y)	\
	case offsetof(ExtVexGuestARMState, guest_##y) ... 3+offsetof(VexGuestARMState, guest_##y): \
	return #x;
#define CASE_OFF2NAME(x)				\
	CASE_OFF2NAME_4(ExtVexGuestARMState, guest_##x)	\
	return #x;
	CASE_OFF2NAME(LINKED)
	default: return NULL;
	}
	return NULL;
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

