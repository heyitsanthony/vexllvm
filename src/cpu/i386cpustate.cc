#include <stdio.h>
#include "Sugar.h"

#include <iostream>
#include <llvm/Intrinsics.h>
#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <cstring>
#include <vector>

#include "guest.h"
#include "guesttls.h"
#include "i386cpustate.h"
extern "C" {
#include <valgrind/libvex_guest_x86.h>
}

using namespace llvm;

#define state2i386()	((VexGuestX86State*)(state_data))

I386CPUState::I386CPUState()
{
	mkRegCtx();

	state_data = new uint8_t[state_byte_c+1];
	memset(state_data, 0, state_byte_c+1);
	exit_type = &state_data[state_byte_c];
	state2i386()->guest_DFLAG = 1;
}

I386CPUState::~I386CPUState()
{
	delete [] state_data;
}

void I386CPUState::setPC(void* ip) {
	state2i386()->guest_EIP = (uintptr_t)ip;
}
void* I386CPUState::getPC(void) const {
	return (void*)state2i386()->guest_EIP;
}


/* ripped from libvex_guest_86 */
static struct guest_ctx_field x86_fields[] =
{
	{32, 8, "GPR"},
	{32, 1, "CC_OP"},
	{32, 1, "CC_DEP1"},
	{32, 1, "CC_DEP2"},
	{32, 1, "CC_NDEP"},

	{32, 1, "DFLAG"},
	{32, 1, "IDFLAG"},
	{32, 1, "ACFLAG"},

	{32, 1, "EIP"},

	{64, 8, "FPREG"},
	{8, 8, "FPTAG"},
	{32, 1, "FPROUND"},
	{32, 1, "FC3210"},
	{32, 1, "FTOP"},

	{32, 1, "SSEROUND"},
	{128, 8, "XMM"},


	{16, 1, "CS"},
	{16, 1, "DS"},
	{16, 1, "ES"},
	{16, 1, "FS"},
	{16, 1, "GS"},
	{16, 1, "SS"},

	{sizeof(void*)*8, 1, "LDT"},
	{sizeof(void*)*8, 1, "GDT"},

	{32, 1, "EMWARN"},

	{32, 1, "TISTART"},
	{32, 1, "TILEN"},

	/* unredirected guest addr at start of translation whose
	 * start has been redirected */
	{32, 1, "NRADDR"},

	/* darwin hax */
	{32, 1, "SC_CLASS"},

	{32, 1, "IP_AT_SYSCALL"},


	{32, 3, "pad"},
	/* END VEX STRUCTURE */

	{0}	/* time to stop */
};

void I386CPUState::mkRegCtx(void)
{
	guestCtxTy = mkFromFields(x86_fields, off2ElemMap);
}

extern void dumpIRSBs(void);


const char* I386CPUState::off2Name(unsigned int off) const
{
	switch (off) {
#define CASE_OFF2NAME(x)	\
	case offsetof(VexGuestX86State, guest_##x) ... 3+offsetof(VexGuestX86State, guest_##x): \
	return #x;

	CASE_OFF2NAME(EAX)
	CASE_OFF2NAME(ECX)
	CASE_OFF2NAME(EDX)
	CASE_OFF2NAME(EBX)
	CASE_OFF2NAME(ESP)
	CASE_OFF2NAME(EBP)
	CASE_OFF2NAME(ESI)
	CASE_OFF2NAME(EDI)
	CASE_OFF2NAME(CC_OP)
	CASE_OFF2NAME(CC_DEP1)
	CASE_OFF2NAME(CC_DEP2)
	CASE_OFF2NAME(CC_NDEP)
	CASE_OFF2NAME(DFLAG)
	CASE_OFF2NAME(IDFLAG)
	CASE_OFF2NAME(ACFLAG)
	CASE_OFF2NAME(EIP)
	CASE_OFF2NAME(FPROUND)
	CASE_OFF2NAME(FC3210)
	CASE_OFF2NAME(FTOP)
	CASE_OFF2NAME(SSEROUND)
	CASE_OFF2NAME(XMM0)
	CASE_OFF2NAME(XMM1)
	CASE_OFF2NAME(XMM2)
	CASE_OFF2NAME(XMM3)
	CASE_OFF2NAME(XMM4)
	CASE_OFF2NAME(XMM5)
	CASE_OFF2NAME(XMM6)
	CASE_OFF2NAME(XMM7)
	
#undef CASE_OFF2NAME
#define CASE_OFF2NAME(x)	\
	case offsetof(VexGuestX86State, guest_##x) ... 1+offsetof(VexGuestX86State, guest_##x): \
	return #x;
	CASE_OFF2NAME(CS)
	CASE_OFF2NAME(DS)
	CASE_OFF2NAME(ES)
	CASE_OFF2NAME(FS)
	CASE_OFF2NAME(GS)
	CASE_OFF2NAME(SS)
	
#undef CASE_OFF2NAME	
#define CASE_OFF2NAME(x)	\
	case offsetof(VexGuestX86State, guest_##x) ... sizeof(void*) - 1 + offsetof(VexGuestX86State, guest_##x): \
	return #x;
	CASE_OFF2NAME(LDT)
	CASE_OFF2NAME(GDT)
	default: return NULL;
	}
	return NULL;
}

/* gets the element number so we can do a GEP */
unsigned int I386CPUState::byteOffset2ElemIdx(unsigned int off) const
{
	byte2elem_map::const_iterator it;
	it = off2ElemMap.find(off);
	if (it == off2ElemMap.end()) {
		unsigned int	c = 0;
		fprintf(stderr, "WTF IS AT %d\n", off);
		dumpIRSBs();
		for (int i = 0; x86_fields[i].f_len; i++) {
			fprintf(stderr, "%s@%d\n", x86_fields[i].f_name, c);
			c += (x86_fields[i].f_len/8)*
				x86_fields[i].f_count;
		}
		assert (0 == 1 && "Could not resolve byte offset");
	}
	return (*it).second;
}

void I386CPUState::setStackPtr(void* stack_ptr)
{
	state2i386()->guest_ESP = (uint64_t)stack_ptr;
}

void* I386CPUState::getStackPtr(void) const
{
	return (void*)(state2i386()->guest_ESP);
}

SyscallParams I386CPUState::getSyscallParams(void) const
{
	return SyscallParams(
		state2i386()->guest_EAX,
		state2i386()->guest_EBX,
		state2i386()->guest_ECX,
		state2i386()->guest_EDX,
		state2i386()->guest_ESI,
		state2i386()->guest_EDI,
		state2i386()->guest_EBP);
}


void I386CPUState::setSyscallResult(uint64_t ret)
{
	state2i386()->guest_EAX = ret;
}

uint64_t I386CPUState::getExitCode(void) const
{
	/* exit code is from call to exit(), which passes the exit
	 * code through the first argument */
	return state2i386()->guest_EBX;
}

// 152 == XMM base
#define get_xmm_lo(i)	((uint64_t*)(&(((uint8_t*)state_data)[152+16*i])))[0]
#define get_xmm_hi(i)	((uint64_t*)(&(((uint8_t*)state_data)[152+16*i])))[1]

void I386CPUState::print(std::ostream& os) const
{
	os << "EIP: " << (void*)state2i386()->guest_EIP << "\n";
	os << "EAX: " << (void*)state2i386()->guest_EAX << "\n";
	os << "EBX: " << (void*)state2i386()->guest_EBX << "\n";
	os << "ECX: " << (void*)state2i386()->guest_ECX << "\n";
	os << "EDX: " << (void*)state2i386()->guest_EDX << "\n";
	os << "ESP: " << (void*)state2i386()->guest_ESP << "\n";
	os << "EBP: " << (void*)state2i386()->guest_EBP << "\n";
	os << "EDI: " << (void*)state2i386()->guest_EDI << "\n";
	os << "ESI: " << (void*)state2i386()->guest_ESI << "\n";

	for (int i = 0; i < 8; i++) {
		os
		<< "XMM" << i << ": "
		<< (void*) get_xmm_hi(i) << "|"
		<< (void*)get_xmm_lo(i) << std::endl;
	}

	for (int i = 0; i < 8; i++) {
		int r  = (state2i386()->guest_FTOP + i) & 0x7;
		os
		<< "ST" << i << ": "
		<< (void*)state2i386()->guest_FPREG[r] << std::endl;
	}
}

/* set a function argument */
void I386CPUState::setFuncArg(uintptr_t arg_val, unsigned int arg_num)
{
	assert(!"allowed to set args on x86, no std calling convention");
}

#ifdef __i386__
void I386CPUState::setRegs(const user_regs_struct& regs, 
	const user_fpregs_struct& fpregs) {
	state2i386()->guest_EAX = regs.eax;
	state2i386()->guest_ECX = regs.ecx;
	state2i386()->guest_EDX = regs.edx;
	state2i386()->guest_EBX = regs.ebx;
	state2i386()->guest_ESP = regs.esp;
	state2i386()->guest_EBP = regs.ebp;
	state2i386()->guest_ESI = regs.esi;
	state2i386()->guest_EDI = regs.edi;
	state2i386()->guest_EIP = regs.eip;

	//TODO: some kind of eflags, checking but i don't yet understand this
	//mess of broken apart state.

	//TODO: segments? valgrind does something less frakked up here 
	//(than amd64) ... there are actual segments... and no fs zero

	memcpy(&state2i386()->guest_XMM0, &fpregs.xmm_space[0], sizeof(fpregs.xmm_space));

	//TODO: this is surely wrong, the sizes don't even match...
	memcpy(&state2i386()->guest_FPREG[0], &fpregs.st_space[0], sizeof(state2i386()->guest_FPREG));

	//TODO: floating point flags and extra fp state, sse  rounding
}
#endif