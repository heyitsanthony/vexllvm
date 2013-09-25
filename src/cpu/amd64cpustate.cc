#include <stdio.h>
#include "Sugar.h"
#include <stddef.h>
#include <iostream>
#include <cstring>
#include <vector>

#include "cpu/amd64cpustate.h"

const char* AMD64CPUState::abi_linux_scregs[] =
{"RAX", "RDI", "RSI", "RDX", "R10", "R8", "R9", NULL};

#define state2amd64()	((VexGuestAMD64State*)(state_data))

AMD64CPUState::AMD64CPUState()
{
	state_byte_c = getFieldsSize(getFields());
	state_data = new uint8_t[state_byte_c+1];
	memset(state_data, 0, state_byte_c+1);
	exit_type = &state_data[state_byte_c];
	state2amd64()->guest_DFLAG = 1;
	state2amd64()->guest_CC_OP = 0 /* AMD64G_CC_OP_COPY */;
	state2amd64()->guest_CC_DEP1 = (1 << 2);
}

AMD64CPUState::~AMD64CPUState() { delete [] state_data; }

void AMD64CPUState::setPC(guest_ptr ip) {
	state2amd64()->guest_RIP = ip;
}

guest_ptr AMD64CPUState::getPC(void) const {
	return guest_ptr(state2amd64()->guest_RIP);
}

/* ripped from libvex_guest_amd64 */
static struct guest_ctx_field amd64_fields[] =
{
	{64, 1, "EvC_FAILADDR"},
	{32, 1, "EvC_COUNTER"},
	{32, 1, "EvC_PAD"},
//	{64, 16, "GPR"},	/* 0-15, 8*16 = 128*/
	{64, 1, "RAX"},
	{64, 1, "RCX"},
	{64, 1, "RDX"},
	{64, 1, "RBX"},
	{64, 1, "RSP"},
	{64, 1, "RBP"},
	{64, 1, "RSI"},
	{64, 1, "RDI"},
	{64, 1, "R8"},
	{64, 1, "R9"},
	{64, 1, "R10"},
	{64, 1, "R11"},
	{64, 1, "R12"},
	{64, 1, "R13"},
	{64, 1, "R14"},
	{64, 1, "R15"},

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
	{256, 17, "YMM"},	/* there is an YMM16 for valgrind stuff */

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

	{64, 1, "pad1"},
	/* END VEX STRUCTURE */

	{0}	/* time to stop */
};

const struct guest_ctx_field* AMD64CPUState::getFields(void) const
{ return amd64_fields; }


extern void dumpIRSBs(void);

const char* AMD64CPUState::off2Name(unsigned int off) const
{
	switch (off) {
#define CASE_OFF2NAME(x)	\
	case offsetof(VexGuestAMD64State, guest_##x) ... (7+offsetof(VexGuestAMD64State, guest_##x)): \
	return #x;
#define CASE_OFF2NAMEN(x,y)	\
	case offsetof(VexGuestAMD64State, guest_##x##y) ... (7+offsetof(VexGuestAMD64State, guest_##x##y)): \
	return #x"["#y"]";


	CASE_OFF2NAME(RAX)
	CASE_OFF2NAME(RCX)
	CASE_OFF2NAME(RDX)
	CASE_OFF2NAME(RBX)
	CASE_OFF2NAME(RSP)
	CASE_OFF2NAME(RBP)
	CASE_OFF2NAME(RSI)
	CASE_OFF2NAME(RDI)
	CASE_OFF2NAMEN(R,8)
	CASE_OFF2NAMEN(R,9)
	CASE_OFF2NAMEN(R,10)
	CASE_OFF2NAMEN(R,11)
	CASE_OFF2NAMEN(R,12)
	CASE_OFF2NAMEN(R,13)
	CASE_OFF2NAMEN(R,14)
	CASE_OFF2NAMEN(R,15)
	CASE_OFF2NAME(CC_OP)
	CASE_OFF2NAMEN(CC_DEP,1)
	CASE_OFF2NAMEN(CC_DEP,2)
	CASE_OFF2NAME(CC_NDEP)
	CASE_OFF2NAME(DFLAG)
	CASE_OFF2NAME(RIP)
	CASE_OFF2NAME(ACFLAG)
	CASE_OFF2NAME(IDFLAG)
	CASE_OFF2NAME(SSEROUND)
	CASE_OFF2NAMEN(YMM,0)
	CASE_OFF2NAMEN(YMM,1)
	CASE_OFF2NAMEN(YMM,2)
	CASE_OFF2NAMEN(YMM,3)
	CASE_OFF2NAMEN(YMM,4)
	CASE_OFF2NAMEN(YMM,5)
	CASE_OFF2NAMEN(YMM,6)
	CASE_OFF2NAMEN(YMM,7)
	CASE_OFF2NAMEN(YMM,8)
	CASE_OFF2NAMEN(YMM,9)
	CASE_OFF2NAMEN(YMM,10)
	CASE_OFF2NAMEN(YMM,11)
	CASE_OFF2NAMEN(YMM,12)
	CASE_OFF2NAMEN(YMM,13)
	CASE_OFF2NAMEN(YMM,14)
	CASE_OFF2NAMEN(YMM,15)
	CASE_OFF2NAMEN(YMM,16)
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

void AMD64CPUState::setStackPtr(guest_ptr stack_ptr)
{ state2amd64()->guest_RSP = (uint64_t)stack_ptr; }

guest_ptr AMD64CPUState::getStackPtr(void) const
{ return guest_ptr(state2amd64()->guest_RSP); }

#define YMM_BASE	offsetof(VexGuestAMD64State, guest_YMM0)
/* 32 because of YMM / AVX extensions */
#define get_xmm_lo(x,i)	((const uint64_t*)(	\
	&(((const uint8_t*)s)[YMM_BASE+32*i])))[0]
#define get_xmm_hi(x,i)	((const uint64_t*)(	\
	&(((const uint8_t*)s)[YMM_BASE+32*i])))[1]
#define get_ymm_lo(x,i)	((const uint64_t*)(	\
	&(((const uint8_t*)s)[YMM_BASE+32*i])))[2]
#define get_ymm_hi(x,i)	((const uint64_t*)(	\
	&(((const uint8_t*)s)[YMM_BASE+32*i])))[3]


void AMD64CPUState::print(std::ostream& os, const void* regctx) const
{
	VexGuestAMD64State	*s;

	s = const_cast<VexGuestAMD64State*>(
		(const VexGuestAMD64State*)regctx);

	os << "RIP: " << (void*)s->guest_RIP << "\n";
	os << "RAX: " << (void*)s->guest_RAX << "\n";
	os << "RBX: " << (void*)s->guest_RBX << "\n";
	os << "RCX: " << (void*)s->guest_RCX << "\n";
	os << "RDX: " << (void*)s->guest_RDX << "\n";
	os << "RSP: " << (void*)s->guest_RSP << "\n";
	os << "RBP: " << (void*)s->guest_RBP << "\n";
	os << "RDI: " << (void*)s->guest_RDI << "\n";
	os << "RSI: " << (void*)s->guest_RSI << "\n";
	os << "R8: " << (void*)s->guest_R8 << "\n";
	os << "R9: " << (void*)s->guest_R9 << "\n";
	os << "R10: " << (void*)s->guest_R10 << "\n";
	os << "R11: " << (void*)s->guest_R11 << "\n";
	os << "R12: " << (void*)s->guest_R12 << "\n";
	os << "R13: " << (void*)s->guest_R13 << "\n";
	os << "R14: " << (void*)s->guest_R14 << "\n";
	os << "R15: " << (void*)s->guest_R15 << "\n";

	os << "EFLAGS: " << (void*)LibVEX_GuestAMD64_get_rflags(s) << '\n';

	for (int i = 0; i < 16; i++) {
		os
		<< "YMM" << i
		<< ": "<< (void*) get_xmm_hi(s,i)
		<< "|" << (void*)get_xmm_lo(s,i)
		<< "(" << (void*)get_ymm_hi(s,i)
		<< "|" << (void*)get_ymm_lo(s,i)
		<< ")" << std::endl;
	}

	for (int i = 0; i < 8; i++) {
		int r  = (state2amd64()->guest_FTOP + i) & 0x7;
		os
		<< "ST" << i << ": "
		<< (void*)s->guest_FPREG[r] << std::endl;
	}

	os << "fs_base = " << (void*)s->guest_FS_ZERO << std::endl;
}

void AMD64CPUState::setFSBase(uintptr_t base)
{ state2amd64()->guest_FS_ZERO = base; }

uintptr_t AMD64CPUState::getFSBase() const
{ return state2amd64()->guest_FS_ZERO; }

static const int arg2reg[] =
{
	offsetof(VexGuestAMD64State, guest_RDI),
	offsetof(VexGuestAMD64State, guest_RSI),
	offsetof(VexGuestAMD64State, guest_RDX),
	offsetof(VexGuestAMD64State, guest_RCX),
	offsetof(VexGuestAMD64State, guest_R8),
	offsetof(VexGuestAMD64State, guest_R9)
};

/* set a function argument */
void AMD64CPUState::setFuncArg(uintptr_t arg_val, unsigned int arg_num)
{
	assert (arg_num <= 5);
	*((uint64_t*)((uintptr_t)state_data + arg2reg[arg_num])) = arg_val;
}

#ifdef __amd64__
void AMD64CPUState::setRegs(
	VexGuestAMD64State& v,
	const user_regs_struct& regs, 
	const user_fpregs_struct& fpregs)
{
	v.guest_RAX = regs.rax;
	v.guest_RCX = regs.rcx;
	v.guest_RDX = regs.rdx;
	v.guest_RBX = regs.rbx;
	v.guest_RSP = regs.rsp;
	v.guest_RBP = regs.rbp;
	v.guest_RSI = regs.rsi;
	v.guest_RDI = regs.rdi;
	v.guest_R8  = regs.r8;
	v.guest_R9  = regs.r9;
	v.guest_R10 = regs.r10;
	v.guest_R11 = regs.r11;
	v.guest_R12 = regs.r12;
	v.guest_R13 = regs.r13;
	v.guest_R14 = regs.r14;
	v.guest_R15 = regs.r15;
	v.guest_RIP = regs.rip;

	//TODO: some kind of eflags, checking but i don't yet understand this
	//mess of broken apart state.

	//valgrind/vex seems to not really fully segments them, how sneaky
	assert (regs.fs_base != 0 && "TLS is important to have!!!");
	v.guest_FS_ZERO = regs.fs_base;

	/* XXX: in the future we want to slurp the full YMM registers */
	/* definitely need smarter ptrace code GETREGSET */
	for (unsigned i = 0; i < 16; i++) {
		memcpy( ((char*)&v.guest_YMM0) + i*32,
			((const char*)&fpregs.xmm_space) + i*16,
			16);
	}


	//TODO: this is surely wrong, the sizes don't even match...
	memcpy(&v.guest_FPREG[0], &fpregs.st_space[0], sizeof(v.guest_FPREG));

	//TODO: floating point flags and extra fp state, sse  rounding
	//

	v.guest_CC_OP = 0 /* AMD64G_CC_OP_COPY */;
	v.guest_CC_DEP1 = regs.eflags & (0xff | (3 << 10)) ;
}


void AMD64CPUState::setRegs(
	const user_regs_struct& regs,
	const user_fpregs_struct& fpregs)
{ setRegs(*state2amd64(), regs, fpregs); }
#endif

unsigned int AMD64CPUState::getFuncArgOff(unsigned int arg_num) const
{
	assert (arg_num <= 5);
	return arg2reg[arg_num];
}

unsigned int AMD64CPUState::getStackRegOff(void) const
{ return offsetof(VexGuestAMD64State, guest_RSP); }

unsigned int AMD64CPUState::getRetOff(void) const
{ return offsetof(VexGuestAMD64State, guest_RAX); }

unsigned int AMD64CPUState::getPCOff(void) const
{ return offsetof(VexGuestAMD64State, guest_RIP); }

/* after a syscall, rcx and r11 are reset by the linux kernel */
void AMD64CPUState::resetSyscall(void)
{
	state2amd64()->guest_RCX = 0;
	state2amd64()->guest_R11 = 0;
}



#define CPU2GDB_DECL(x)	offsetof(VexGuestAMD64State, guest_##x)
int cpu2gdb_gpr[16] =
{
CPU2GDB_DECL(RAX),
CPU2GDB_DECL(RBX),
CPU2GDB_DECL(RCX),
CPU2GDB_DECL(RDX),
CPU2GDB_DECL(RSI),
CPU2GDB_DECL(RDI),
CPU2GDB_DECL(RBP),
CPU2GDB_DECL(RSP),
CPU2GDB_DECL(R8),
CPU2GDB_DECL(R9),
CPU2GDB_DECL(R10),
CPU2GDB_DECL(R11),
CPU2GDB_DECL(R12),
CPU2GDB_DECL(R13),
CPU2GDB_DECL(R14),
CPU2GDB_DECL(R15)
};

int AMD64CPUState::cpu2gdb(int gdb_off) const
{
	if (gdb_off < 8*16)
		return cpu2gdb_gpr[gdb_off/8] + (gdb_off % 8);

	if (gdb_off < 8*17)	/* rip */
		return CPU2GDB_DECL(RIP) + (gdb_off % 8);

	if (gdb_off < 8*18)	/* rflags */
		return -2;	/* virtual value; no offset. how to populate? */

	/* cs, ss, ds, es, fs, gs */
	// uint16_t cpu2gdb_seg[6] = {0x33, 0x2b, 0, 0, 0, 0};
	if (gdb_off < (8*19 + 6*2))
		return -2;	/* again; virtual-- we don't know it */

	/* out of reg */
	return -1;
}
