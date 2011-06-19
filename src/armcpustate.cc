#include <stdio.h>
#include "Sugar.h"

#include <iostream>
#include <llvm/Intrinsics.h>
#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <cstring>
#include <vector>

#include "guesttls.h"
#include "armcpustate.h"
extern "C" {
#include <valgrind/libvex_guest_arm.h>
}

using namespace llvm;

#define state2arm()	((VexGuestARMState*)(state_data))

#define FS_SEG_OFFSET	(24*8)

ARMCPUState::ARMCPUState()
{
	mkRegCtx();

	state_data = new uint8_t[state_byte_c+1];
	memset(state_data, 0, state_byte_c+1);
	exit_type = &state_data[state_byte_c];
}

ARMCPUState::~ARMCPUState()
{
	delete [] state_data;
}

void ARMCPUState::setPC(void* ip) {
	state2arm()->guest_R15T = (uintptr_t)ip;
}
void* ARMCPUState::getPC(void) const {
	/* todo is this right, do we need to mask the low
	   bits that control whether its in thumb mode or not? */
	return (void*)state2arm()->guest_R15T;
}


/* ripped from libvex_guest_arm */
static struct guest_ctx_field arm_fields[] =
{
	{32, 16, "GPR"},
	{32, 1, "CC_OP"},
	{32, 1, "CC_DEP1"},
	{32, 1, "CC_DEP2"},
	{32, 1, "CC_NDEP"},

	{32, 1, "QFLAG32"},

	{32, 1, "GEFLAG0"},
	{32, 1, "GEFLAG1"},
	{32, 1, "GEFLAG2"},
	{32, 1, "GEFLAG3"},
	
	{32, 1, "EMWARN"},
	
	{32, 1, "TISTART"},
	{32, 1, "TILEN"},

	/* unredirected guest addr at start of translation whose
	 * start has been redirected */
	{32, 1, "NRADDR"},

	{32, 1, "IP_AT_SYSCALL"},

	{64, 32, "VFP_R"},
	{32, 1, "FPSCR"},

	{32, 1, "TPIDRURO"},

	{32, 1, "ITSTATE"},

	{32, 3, "pad"},
	/* END VEX STRUCTURE */

	{0}	/* time to stop */
};

void ARMCPUState::mkRegCtx(void)
{
	guestCtxTy = mkFromFields(arm_fields, off2ElemMap);
}

extern void dumpIRSBs(void);


const char* ARMCPUState::off2Name(unsigned int off) const
{
	switch (off) {
#define CASE_OFF2NAME2(x,y)	\
	case offsetof(VexGuestARMState, guest_##y) ... 3+offsetof(VexGuestARMState, guest_##y): \
	return #x;
#define CASE_OFF2NAME(x)	\
	case offsetof(VexGuestARMState, guest_##x) ... 3+offsetof(VexGuestARMState, guest_##x): \
	return #x;

	CASE_OFF2NAME(R0)
	CASE_OFF2NAME(R1)
	CASE_OFF2NAME(R2)
	CASE_OFF2NAME(R3)
	CASE_OFF2NAME(R4)
	CASE_OFF2NAME(R5)
	CASE_OFF2NAME(R6)
	CASE_OFF2NAME(R7)
	CASE_OFF2NAME(R8)
	CASE_OFF2NAME(R9)
	CASE_OFF2NAME(R10)
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
	CASE_OFF2NAME(EMWARN)
	CASE_OFF2NAME(TISTART)
	CASE_OFF2NAME(TILEN)
	CASE_OFF2NAME(NRADDR)
	CASE_OFF2NAME(IP_AT_SYSCALL)
	CASE_OFF2NAME(D0)
	CASE_OFF2NAME(D1)
	CASE_OFF2NAME(D2)
	CASE_OFF2NAME(D3)
	CASE_OFF2NAME(D4)
	CASE_OFF2NAME(D5)
	CASE_OFF2NAME(D6)
	CASE_OFF2NAME(D7)
	CASE_OFF2NAME(D8)
	CASE_OFF2NAME(D9)
	CASE_OFF2NAME(D10)
	CASE_OFF2NAME(D11)
	CASE_OFF2NAME(D12)
	CASE_OFF2NAME(D13)
	CASE_OFF2NAME(D14)
	CASE_OFF2NAME(D15)
	CASE_OFF2NAME(D16)
	CASE_OFF2NAME(D17)
	CASE_OFF2NAME(D18)
	CASE_OFF2NAME(D19)
	CASE_OFF2NAME(D20)
	CASE_OFF2NAME(D21)
	CASE_OFF2NAME(D22)
	CASE_OFF2NAME(D23)
	CASE_OFF2NAME(D24)
	CASE_OFF2NAME(D25)
	CASE_OFF2NAME(D26)
	CASE_OFF2NAME(D27)
	CASE_OFF2NAME(D28)
	CASE_OFF2NAME(D29)
	CASE_OFF2NAME(D30)
	CASE_OFF2NAME(D31)
	CASE_OFF2NAME(FPSCR)
	CASE_OFF2NAME(TPIDRURO)
	CASE_OFF2NAME(ITSTATE)
	default: return NULL;
	}
	return NULL;
}

/* gets the element number so we can do a GEP */
unsigned int ARMCPUState::byteOffset2ElemIdx(unsigned int off) const
{
	byte2elem_map::const_iterator it;
	it = off2ElemMap.find(off);
	if (it == off2ElemMap.end()) {
		unsigned int	c = 0;
		fprintf(stderr, "WTF IS AT %d\n", off);
		dumpIRSBs();
		for (int i = 0; arm_fields[i].f_len; i++) {
			fprintf(stderr, "%s@%d\n", arm_fields[i].f_name, c);
			c += (arm_fields[i].f_len/8)*
				arm_fields[i].f_count;
		}
		assert (0 == 1 && "Could not resolve byte offset");
	}
	return (*it).second;
}

void ARMCPUState::setStackPtr(void* stack_ptr)
{
	state2arm()->guest_R13 = (uint64_t)stack_ptr;
}

void* ARMCPUState::getStackPtr(void) const
{
	return (void*)(state2arm()->guest_R13);
}

SyscallParams ARMCPUState::getSyscallParams(void) const
{
	/* really figure out what this is... assuming they did the obvious */
	return SyscallParams(
		state2arm()->guest_R7,
		state2arm()->guest_R0,
		state2arm()->guest_R1,
		state2arm()->guest_R2,
		state2arm()->guest_R3,
		state2arm()->guest_R4,
		state2arm()->guest_R5);
}


void ARMCPUState::setSyscallResult(uint64_t ret)
{
	state2arm()->guest_R0 = ret;
}

uint64_t ARMCPUState::getExitCode(void) const
{
	/* exit code is from call to exit(), which passes the exit
	 * code through the first argument */
	return state2arm()->guest_R1;
}

void ARMCPUState::print(std::ostream& os) const
{

	unsigned int *gpr_base = &state2arm()->guest_R0;
	for (int i = 0; i < 16; i++) {
		os
		<< "R" << i << ": "
		<< (void*)gpr_base[i] << std::endl;
	}

	unsigned long long *vpr_base = &state2arm()->guest_D0;
	for (int i = 0; i < 16; i++) {
		os
		<< "D" << i << ": "
		<< (void*)vpr_base[i] << std::endl;
	}
	os << "FPCSR: "  << (void*)state2arm()->guest_FPSCR << "\n";
	/* tls */
	os << "TPIDRURO: "  << (void*)state2arm()->guest_TPIDRURO << "\n";
}

/* set a function argument */
void ARMCPUState::setFuncArg(uintptr_t arg_val, unsigned int arg_num)
{
	const int arg2reg[] = {
		offsetof(VexGuestARMState, guest_R0),
		offsetof(VexGuestARMState, guest_R1),
		offsetof(VexGuestARMState, guest_R2),
		offsetof(VexGuestARMState, guest_R3)
		};

	assert (arg_num <= 3);
	*((uint64_t*)((uintptr_t)state_data + arg2reg[arg_num])) = arg_val;
}

#ifdef __arm__
void ARMCPUState::setRegs(const user& regs, const user_fp& fpregs, 
	const user_vfp& vfpregs) {
	state2arm()->guest_R0 = regs.rax;
	state2arm()->guest_R1 = regs.rcx;
	state2arm()->guest_R2 = regs.rdx;
	state2arm()->guest_R3 = regs.rbx;
	state2arm()->guest_R4 = regs.rsp;
	state2arm()->guest_R5 = regs.rbp;
	state2arm()->guest_R6 = regs.rsi;
	state2arm()->guest_R7 = regs.rdi;
	state2arm()->guest_R8  = regs.r8;
	state2arm()->guest_R9  = regs.r9;
	state2arm()->guest_R10 = regs.r10;
	state2arm()->guest_R11 = regs.fp;
	state2arm()->guest_R12 = regs.ip;
	state2arm()->guest_R13 = regs.sp;
	state2arm()->guest_R14 = regs.lr;
	state2arm()->guest_R15 = regs.pc;

	//TODO: some kind of flags, checking but i don't yet understand this
	//mess of broken apart state.


	//TODO: vector data registers
	//TODO: float registers


}
#endif