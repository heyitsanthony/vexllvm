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

MIPS32CPUState::MIPS32CPUState()
{
	state_byte_c = getFieldsSize(getFields());
	state_data = new uint8_t[state_byte_c+1];
	memset(state_data, 0, state_byte_c+1);
	exit_type = &state_data[state_byte_c];
}

MIPS32CPUState::~MIPS32CPUState() { delete [] state_data; }

void MIPS32CPUState::setPC(guest_ptr ip) { state2mips32()->guest_PC = ip; }

guest_ptr MIPS32CPUState::getPC(void) const
{ return guest_ptr(state2mips32()->guest_PC); }

static struct guest_ctx_field mips32_fields[] =
{
	{32, 32, "GPR"},
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

	{32, 1, "TISTART"},
	{32, 1, "TILEN"},
	{32, 1, "NRADDR"},

	{32, 1, "FAILADDR"},
	{32, 1, "COUNTER"},
	{32, 1, "COND"},

	{32, 5, "pad"},
	/* END VEX STRUCTURE */

	{0}	/* time to stop */
};


const struct guest_ctx_field* MIPS32CPUState::getFields(void) const
{ return mips32_fields; }

extern void dumpIRSBs(void);


const char* MIPS32CPUState::off2Name(unsigned int off) const
{
	switch (off) {
#define CASE_OFF2NAME(x)	\
	case offsetof(VexGuestMIPS32State, guest_##x) ... 3+offsetof(VexGuestMIPS32State, guest_##x): \
	return #x;

	CASE_OFF2NAME(r0) CASE_OFF2NAME(r1) CASE_OFF2NAME(r2)
	CASE_OFF2NAME(r3) CASE_OFF2NAME(r4) CASE_OFF2NAME(r5)
	CASE_OFF2NAME(r6) CASE_OFF2NAME(r7) CASE_OFF2NAME(r8)
	CASE_OFF2NAME(r9) CASE_OFF2NAME(r10) CASE_OFF2NAME(r12)
	CASE_OFF2NAME(r13) CASE_OFF2NAME(r14) CASE_OFF2NAME(r15)
	CASE_OFF2NAME(r16) CASE_OFF2NAME(r17) CASE_OFF2NAME(r18)
	CASE_OFF2NAME(r19) CASE_OFF2NAME(r20) CASE_OFF2NAME(r21)
	CASE_OFF2NAME(r22) CASE_OFF2NAME(r23) CASE_OFF2NAME(r24)
	CASE_OFF2NAME(r25) CASE_OFF2NAME(r26) CASE_OFF2NAME(r27)
	CASE_OFF2NAME(r28) CASE_OFF2NAME(r29) CASE_OFF2NAME(r30)
	CASE_OFF2NAME(r31)

	CASE_OFF2NAME(PC)
	CASE_OFF2NAME(HI)
	CASE_OFF2NAME(LO)
	CASE_OFF2NAME(FIR)
	CASE_OFF2NAME(FCCR)
	CASE_OFF2NAME(FEXR)
	CASE_OFF2NAME(FENR)
	CASE_OFF2NAME(FCSR)

	CASE_OFF2NAME(f0) CASE_OFF2NAME(f1) CASE_OFF2NAME(f2)
	CASE_OFF2NAME(f3) CASE_OFF2NAME(f4) CASE_OFF2NAME(f5)
	CASE_OFF2NAME(f6) CASE_OFF2NAME(f7) CASE_OFF2NAME(f8)
	CASE_OFF2NAME(f9) CASE_OFF2NAME(f10) CASE_OFF2NAME(f12)
	CASE_OFF2NAME(f13) CASE_OFF2NAME(f14) CASE_OFF2NAME(f15)
	CASE_OFF2NAME(f16) CASE_OFF2NAME(f17) CASE_OFF2NAME(f18)
	CASE_OFF2NAME(f19) CASE_OFF2NAME(f20) CASE_OFF2NAME(f21)
	CASE_OFF2NAME(f22) CASE_OFF2NAME(f23) CASE_OFF2NAME(f24)
	CASE_OFF2NAME(f25) CASE_OFF2NAME(f26) CASE_OFF2NAME(f27)
	CASE_OFF2NAME(f28) CASE_OFF2NAME(f29) CASE_OFF2NAME(f30)
	CASE_OFF2NAME(f31)


	CASE_OFF2NAME(ULR)
	CASE_OFF2NAME(EMWARN)
	CASE_OFF2NAME(TISTART)
	CASE_OFF2NAME(TILEN)
	CASE_OFF2NAME(NRADDR)

//	CASE_OFF2NAME(FAILADDR)
//	CASE_OFF2NAME(COUNTER)
	CASE_OFF2NAME(COND)

	default: return NULL;
	}
	return NULL;
}

/* gets the element number so we can do a GEP */
unsigned int MIPS32CPUState::byteOffset2ElemIdx(unsigned int off) const
{
	byte2elem_map::const_iterator it;
	it = off2ElemMap.find(off);
	if (it == off2ElemMap.end()) {
		unsigned int	c = 0;
		fprintf(stderr, "WTF IS AT %d\n", off);
		dumpIRSBs();
		for (int i = 0; mips32_fields[i].f_len; i++) {
			fprintf(stderr, "%s@%d\n", mips32_fields[i].f_name, c);
			c += (mips32_fields[i].f_len/8)*
				mips32_fields[i].f_count;
		}
		assert (0 == 1 && "Could not resolve byte offset");
	}
	return (*it).second;
}

void MIPS32CPUState::setStackPtr(guest_ptr stack_ptr)
{ state2mips32()->guest_r29 = (uint64_t)stack_ptr; }

guest_ptr MIPS32CPUState::getStackPtr(void) const
{ return guest_ptr(state2mips32()->guest_r29); }

SyscallParams MIPS32CPUState::getSyscallParams(void) const
{
	return SyscallParams(
		state2mips32()->guest_r2, /* v0 */
		state2mips32()->guest_r4, /* a0 */
		state2mips32()->guest_r5, /* a1 */
		state2mips32()->guest_r6, // a2
		state2mips32()->guest_r7, // a3
		state2mips32()->guest_r8, // a4
		state2mips32()->guest_r9 /* a5 */);
}


void MIPS32CPUState::setSyscallResult(uint64_t ret)
{ state2mips32()->guest_r2= ret; }

uint64_t MIPS32CPUState::getExitCode(void) const
{
	/* exit code is from call to exit(), which passes the exit
	 * code through the first argument */
	return state2mips32()->guest_r4;
}

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

/* set a function argument */
void MIPS32CPUState::setFuncArg(uintptr_t arg_val, unsigned int arg_num)
{ assert(!"allowed to set args on mips32, no std calling convention"); }