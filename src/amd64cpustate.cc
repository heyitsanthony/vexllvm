#include <stdio.h>
#include "Sugar.h"

#include <iostream>
#include <cstring>
#include <vector>

#include "guesttls.h"
#include "amd64cpustate.h"
extern "C" {
#include <valgrind/libvex_guest_amd64.h>
}

#define state2amd64()	((VexGuestAMD64State*)(state_data))

#define FS_SEG_OFFSET	(24*8)

AMD64CPUState::AMD64CPUState()
{
	mkRegCtx();

	state_data = new uint8_t[state_byte_c+1];
	memset(state_data, 0, state_byte_c+1);
	exit_type = &state_data[state_byte_c];
	state2amd64()->guest_DFLAG = 1;

	tls = new GuestTLS();
	*((uintptr_t*)(((uintptr_t)state_data) + FS_SEG_OFFSET)) =
		(uintptr_t)tls->getBase();
}

AMD64CPUState::~AMD64CPUState()
{
	delete [] state_data;
	delete tls;
}

void AMD64CPUState::setTLS(GuestTLS* in_tls)
{
	delete tls;
	tls = in_tls;
	*((uintptr_t*)(((uintptr_t)state_data) + FS_SEG_OFFSET)) =
		(uintptr_t)tls->getBase();
}

/* ripped from libvex_guest_amd64 */
static struct guest_ctx_field amd64_fields[] =
{
	{64, 16, "GPR"},	/* 0-15, 8*16 = 128*/
	{64, 1, "CC_OP"},	/* 16, 128 */
	{64, 1, "CC_DEP1"},	/* 17, 136 */
	{64, 1, "CC_DEP2"},	/* 18, 144 */
	{64, 1, "CC_NDEP"},	/* 19, 152 */

	{64, 1, "DFLAG"},	/* 20, 160 */
	{64, 1, "RIP"},		/* 21, 168 */
	{64, 1, "ACFLAG"},	/* 22, 176 */
	{64, 1, "IDFLAG"},	/* 23 */

	{64, 1, "FS_ZERO"},	/* 24 */

	{64, 1, "SSEROUND"},
	{128, 17, "XMM"},	/* there is an XMM16 for valgrind stuff */

	{32, 1, "FTOP"},
	{64, 8, "FPREG"},
	{8, 8, "FPTAG"},

	{64, 1, "FPROUND"},
	{64, 1, "FC3210"},

	{32, 1, "EMWARN"},

	{64, 1, "TISTART"},
	{64, 1, "TILEN"},

	/* unredirected guest addr at start of translation whose
	 * start has been redirected */
	{64, 1, "NRADDR"},

	/* darwin hax */
	{64, 1, "SC_CLASS"},
	{64, 1, "GS_0x60"},
	{64, 1, "IP_AT_SYSCALL"},

	{64, 1, "pad"},
	/* END VEX STRUCTURE */

	{0}	/* time to stop */
};

void AMD64CPUState::mkRegCtx(void)
{
	guestCtxTy = mkFromFields(amd64_fields, off2ElemMap);
}

extern void dumpIRSBs(void);


const char* AMD64CPUState::off2Name(unsigned int off) const
{
	switch (off) {
#define CASE_OFF2NAME(x)	\
	case offsetof(VexGuestAMD64State, guest_##x) ... 7+offsetof(VexGuestAMD64State, guest_##x): \
	return #x;

	CASE_OFF2NAME(RAX)
	CASE_OFF2NAME(RCX)
	CASE_OFF2NAME(RDX)
	CASE_OFF2NAME(RBX)
	CASE_OFF2NAME(RSP)
	CASE_OFF2NAME(RBP)
	CASE_OFF2NAME(RSI)
	CASE_OFF2NAME(RDI)
	CASE_OFF2NAME(R8)
	CASE_OFF2NAME(R9)
	CASE_OFF2NAME(R10)
	CASE_OFF2NAME(R11)
	CASE_OFF2NAME(R12)
	CASE_OFF2NAME(R13)
	CASE_OFF2NAME(R14)
	CASE_OFF2NAME(R15)
	CASE_OFF2NAME(CC_OP)
	CASE_OFF2NAME(CC_DEP1)
	CASE_OFF2NAME(CC_DEP2)
	CASE_OFF2NAME(CC_NDEP)
	CASE_OFF2NAME(DFLAG)
	CASE_OFF2NAME(RIP)
	CASE_OFF2NAME(ACFLAG)
	CASE_OFF2NAME(IDFLAG)
	CASE_OFF2NAME(XMM0)
	CASE_OFF2NAME(XMM1)
	CASE_OFF2NAME(XMM2)
	CASE_OFF2NAME(XMM3)
	CASE_OFF2NAME(XMM4)
	CASE_OFF2NAME(XMM5)
	CASE_OFF2NAME(XMM6)
	CASE_OFF2NAME(XMM7)
	CASE_OFF2NAME(XMM8)
	CASE_OFF2NAME(XMM9)
	CASE_OFF2NAME(XMM10)
	CASE_OFF2NAME(XMM11)
	CASE_OFF2NAME(XMM12)
	CASE_OFF2NAME(XMM13)
	CASE_OFF2NAME(XMM14)
	CASE_OFF2NAME(XMM15)
	CASE_OFF2NAME(XMM16)
	default: return NULL;
	}
	return NULL;
}

/* gets the element number so we can do a GEP */
unsigned int AMD64CPUState::byteOffset2ElemIdx(unsigned int off) const
{
	byte2elem_map::const_iterator it;
	it = off2ElemMap.find(off);
	if (it == off2ElemMap.end()) {
		unsigned int	c = 0;
		fprintf(stderr, "WTF IS AT %d\n", off);
		dumpIRSBs();
		for (int i = 0; amd64_fields[i].f_len; i++) {
			fprintf(stderr, "%s@%d\n", amd64_fields[i].f_name, c);
			c += (amd64_fields[i].f_len/8)*
				amd64_fields[i].f_count;
		}
		assert (0 == 1 && "Could not resolve byte offset");
	}
	return (*it).second;
}

void AMD64CPUState::setStackPtr(void* stack_ptr)
{
	state2amd64()->guest_RSP = (uint64_t)stack_ptr;
}

void* AMD64CPUState::getStackPtr(void) const
{
	return (void*)(state2amd64()->guest_RSP);
}

/**
 * %rax = syscall number
 * rdi, rsi, rdx, r10, r8, r9
 */
SyscallParams AMD64CPUState::getSyscallParams(void) const
{
	return SyscallParams(
		state2amd64()->guest_RAX,
		state2amd64()->guest_RDI,
		state2amd64()->guest_RSI,
		state2amd64()->guest_RDX,
		state2amd64()->guest_R10,
		state2amd64()->guest_R8,
		state2amd64()->guest_R9);
}


void AMD64CPUState::setSyscallResult(uint64_t ret)
{
	state2amd64()->guest_RAX = ret;
}

uint64_t AMD64CPUState::getExitCode(void) const
{
	/* exit code is from call to exit(), which passes the exit
	 * code through the first argument */
	return state2amd64()->guest_RDI;
}

// 208 == XMM base
#define get_xmm_lo(i)	((uint64_t*)(&(((uint8_t*)state_data)[208+16*i])))[0]
#define get_xmm_hi(i)	((uint64_t*)(&(((uint8_t*)state_data)[208+16*i])))[1]

void AMD64CPUState::print(std::ostream& os) const
{
	os << "RIP: " << (void*)state2amd64()->guest_RIP << "\n";
	os << "RAX: " << (void*)state2amd64()->guest_RAX << "\n";
	os << "RBX: " << (void*)state2amd64()->guest_RBX << "\n";
	os << "RCX: " << (void*)state2amd64()->guest_RCX << "\n";
	os << "RDX: " << (void*)state2amd64()->guest_RDX << "\n";
	os << "RSP: " << (void*)state2amd64()->guest_RSP << "\n";
	os << "RBP: " << (void*)state2amd64()->guest_RBP << "\n";
	os << "RDI: " << (void*)state2amd64()->guest_RDI << "\n";
	os << "RSI: " << (void*)state2amd64()->guest_RSI << "\n";
	os << "R8: " << (void*)state2amd64()->guest_R8 << "\n";
	os << "R9: " << (void*)state2amd64()->guest_R9 << "\n";
	os << "R10: " << (void*)state2amd64()->guest_R10 << "\n";
	os << "R11: " << (void*)state2amd64()->guest_R11 << "\n";
	os << "R12: " << (void*)state2amd64()->guest_R12 << "\n";
	os << "R13: " << (void*)state2amd64()->guest_R13 << "\n";
	os << "R14: " << (void*)state2amd64()->guest_R14 << "\n";
	os << "R15: " << (void*)state2amd64()->guest_R15 << "\n";

	for (int i = 0; i < 16; i++) {
		os
		<< "XMM" << i << ": "
		<< (void*) get_xmm_hi(i) << "|"
		<< (void*)get_xmm_lo(i) << std::endl;
	}

	for (int i = 0; i < 8; i++) {
		int r  = (state2amd64()->guest_FTOP + i) & 0x7;
		os
		<< "ST" << i << ": "
		<< (void*)state2amd64()->guest_FPREG[r] << std::endl;
	}

	const uint64_t*	tls_data = (const uint64_t*)tls->getBase();
	unsigned int	tls_bytes = tls->getSize();

	os << "fs_base = " << (void*)state2amd64()->guest_FS_ZERO << std::endl;
#if 0
	for (unsigned int i = 0; i < tls_bytes / sizeof(uint64_t); i++) {
		if (!tls_data[i]) continue;
		os	<< "fs+" << (void*)(i*8)  << ":"
			<< (void*)tls_data[i]
			<< std::endl;
	}
#endif
}

void AMD64CPUState::setFSBase(uintptr_t base) {
	state2amd64()->guest_FS_ZERO = base;
}
uintptr_t AMD64CPUState::getFSBase() const {
	return state2amd64()->guest_FS_ZERO;
}


/* set a function argument */
void AMD64CPUState::setFuncArg(uintptr_t arg_val, unsigned int arg_num)
{
	const int arg2reg[] = {
		offsetof(VexGuestAMD64State, guest_RDI),
		offsetof(VexGuestAMD64State, guest_RSI),
		offsetof(VexGuestAMD64State, guest_RDX),
		offsetof(VexGuestAMD64State, guest_RCX)
		};

	assert (arg_num <= 3);
	*((uint64_t*)((uintptr_t)state_data + arg2reg[arg_num])) = arg_val;
}

#ifdef __amd64__
void AMD64CPUState::setRegs(const user_regs_struct& regs, 
	const user_fpregs_struct& fpregs) {
	state2amd64()->guest_RAX = regs.rax;
	state2amd64()->guest_RCX = regs.rcx;
	state2amd64()->guest_RDX = regs.rdx;
	state2amd64()->guest_RBX = regs.rbx;
	state2amd64()->guest_RSP = regs.rsp;
	state2amd64()->guest_RBP = regs.rbp;
	state2amd64()->guest_RSI = regs.rsi;
	state2amd64()->guest_RDI = regs.rdi;
	state2amd64()->guest_R8  = regs.r8;
	state2amd64()->guest_R9  = regs.r9;
	state2amd64()->guest_R10 = regs.r10;
	state2amd64()->guest_R11 = regs.r11;
	state2amd64()->guest_R12 = regs.r12;
	state2amd64()->guest_R13 = regs.r13;
	state2amd64()->guest_R14 = regs.r14;
	state2amd64()->guest_R15 = regs.r15;
	state2amd64()->guest_RIP = regs.rip;

	//TODO: some kind of eflags, checking but i don't yet understand this
	//mess of broken apart state.

	//valgrind/vex seems to not really fully segments them, how sneaky
	state2amd64()->guest_FS_ZERO = regs.fs_base;

	memcpy(&state2amd64()->guest_XMM0, &fpregs.xmm_space[0], sizeof(fpregs.xmm_space));


	//TODO: this is surely wrong, the sizes don't even match...
	memcpy(&state2amd64()->guest_FPREG[0], &fpregs.st_space[0], sizeof(state2amd64()->guest_FPREG));


	//TODO: floating point flags and extra fp state, sse  rounding
}
#endif