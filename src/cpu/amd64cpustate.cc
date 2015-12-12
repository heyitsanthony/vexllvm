#include <stdio.h>
#include "Sugar.h"
#include <stddef.h>
#include <iostream>
#include <cstring>
#include <vector>

#include "sc_xlate.h"
#include "cpu/amd64cpustate.h"
#include <cstddef>

#define state2amd64()	((VexGuestAMD64State*)(state_data))

#define reg_field_ent_w_n(x, w, n) { #x, w, n, offsetof(VexGuestAMD64State, guest_##x), true }
#define reg_field_ent(x) reg_field_ent_w_n(x, sizeof((VexGuestAMD64State*)0)->guest_##x, 1)
#define raw_field_ent_w_n(x, w, n) { #x, w, n, offsetof(VexGuestAMD64State, x), true }
#define raw_field_ent(x) raw_field_ent_w_n(x, sizeof((VexGuestAMD64State*)0)->x, 1)

/* ripped from libvex_guest_amd64 */
static struct guest_ctx_field amd64_fields[] =
{
	raw_field_ent(host_EvC_FAILADDR),
	raw_field_ent(host_EvC_COUNTER),
	raw_field_ent(pad0),
	reg_field_ent(RAX),
	reg_field_ent(RCX),
	reg_field_ent(RDX),
	reg_field_ent(RBX),
	reg_field_ent(RSP),
	reg_field_ent(RBP),
	reg_field_ent(RSI),
	reg_field_ent(RDI),
	reg_field_ent(R8),
	reg_field_ent(R9),
	reg_field_ent(R10),
	reg_field_ent(R11),
	reg_field_ent(R12),
	reg_field_ent(R13),
	reg_field_ent(R14),
	reg_field_ent(R15),

	reg_field_ent(CC_OP),	/* 16, 128 */
	reg_field_ent(CC_DEP1),	/* 17, 136 */
	reg_field_ent(CC_DEP2),	/* 18, 144 */
	reg_field_ent(CC_NDEP),	/* 19, 152 */

	reg_field_ent(DFLAG),	/* 20, 160 */
	reg_field_ent(RIP),		/* 21, 168 */
	reg_field_ent(ACFLAG),	/* 22, 176 */
	reg_field_ent(IDFLAG),	/* 23 */

	reg_field_ent(FS_ZERO),	/* 24 */

	reg_field_ent(SSEROUND),

	{ "YMM", 32, 17, offsetof(VexGuestAMD64State, guest_YMM0), true },

	reg_field_ent(FTOP),
	reg_field_ent_w_n(FPREG, 8, 8),
	reg_field_ent_w_n(FPTAG, 1, 8),

	reg_field_ent(FPROUND),
	reg_field_ent(FC3210),

	reg_field_ent(EMNOTE),

	/* unredirected guest addr at start of translation whose
	 * start has been redirected */
	reg_field_ent(NRADDR),

	/* darwin hax */
	reg_field_ent(SC_CLASS),
	reg_field_ent(GS_0x60),
	reg_field_ent(IP_AT_SYSCALL),

	raw_field_ent(pad1),
	/* END VEX STRUCTURE */

	{0}	/* time to stop */
};

AMD64CPUState::AMD64CPUState()
	: VexCPUState(amd64_fields)
{
	state_byte_c = sizeof(VexGuestAMD64State);
	state_data = new uint8_t[state_byte_c+1];
	memset(state_data, 0, state_byte_c+1);
	exit_type = &state_data[state_byte_c];
	state2amd64()->guest_DFLAG = 1;
	state2amd64()->guest_CC_OP = 0 /* AMD64G_CC_OP_COPY */;
	state2amd64()->guest_CC_DEP1 = (1 << 2);
	xlate = std::make_unique<AMD64SyscallXlate>();
}

AMD64CPUState::~AMD64CPUState() { delete [] state_data; }

void AMD64CPUState::setPC(guest_ptr ip) {
	state2amd64()->guest_RIP = ip;
}

guest_ptr AMD64CPUState::getPC(void) const {
	return guest_ptr(state2amd64()->guest_RIP);
}

void AMD64CPUState::setStackPtr(guest_ptr stack_ptr)
{ state2amd64()->guest_RSP = (uint64_t)stack_ptr; }

guest_ptr AMD64CPUState::getStackPtr(void) const
{ return guest_ptr(state2amd64()->guest_RSP); }

#define YMM_BASE	offsetof(VexGuestAMD64State, guest_YMM0)
/* 32 because of YMM / AVX extensions */
#define get_xmm_lo(x,i)	((const uint64_t*)(	\
	&(((const uint8_t*)(x))[YMM_BASE+32*i])))[0]
#define get_xmm_hi(x,i)	((const uint64_t*)(	\
	&(((const uint8_t*)(x))[YMM_BASE+32*i])))[1]
#define get_ymm_lo(x,i)	((const uint64_t*)(	\
	&(((const uint8_t*)(x))[YMM_BASE+32*i])))[2]
#define get_ymm_hi(x,i)	((const uint64_t*)(	\
	&(((const uint8_t*)(x))[YMM_BASE+32*i])))[3]

/* XXX: is there a smarter way to do this? */
void AMD64CPUState::print(std::ostream& os, const VexGuestAMD64State& vs)
{
	os << "RIP: " << (void*)vs.guest_RIP << "\n";
	os << "RAX: " << (void*)vs.guest_RAX << "\n";
	os << "RBX: " << (void*)vs.guest_RBX << "\n";
	os << "RCX: " << (void*)vs.guest_RCX << "\n";
	os << "RDX: " << (void*)vs.guest_RDX << "\n";
	os << "RSP: " << (void*)vs.guest_RSP << "\n";
	os << "RBP: " << (void*)vs.guest_RBP << "\n";
	os << "RDI: " << (void*)vs.guest_RDI << "\n";
	os << "RSI: " << (void*)vs.guest_RSI << "\n";
	os << "R8: " << (void*)vs.guest_R8 << "\n";
	os << "R9: " << (void*)vs.guest_R9 << "\n";
	os << "R10: " << (void*)vs.guest_R10 << "\n";
	os << "R11: " << (void*)vs.guest_R11 << "\n";
	os << "R12: " << (void*)vs.guest_R12 << "\n";
	os << "R13: " << (void*)vs.guest_R13 << "\n";
	os << "R14: " << (void*)vs.guest_R14 << "\n";
	os << "R15: " << (void*)vs.guest_R15 << "\n";

	os << "RFLAGS: " << (void*)getRFLAGS(vs) << '\n';

	for (int i = 0; i < 16; i++) {
		os
		<< "YMM" << i
		<< ": "<< (void*)get_xmm_hi(&vs,i)
		<< "|" << (void*)get_xmm_lo(&vs,i)
		<< "(" << (void*)get_ymm_hi(&vs,i)
		<< "|" << (void*)get_ymm_lo(&vs,i)
		<< ")" << std::endl;
	}

	for (int i = 0; i < 8; i++) {
		int r  = (vs.guest_FTOP + i) & 0x7;
		os	<< "ST" << i << ": "
			<< (void*)vs.guest_FPREG[r] << std::endl;
	}
	os << "FPROUND: " << (void*)vs.guest_FPROUND << std::endl;
	os << "FC3210: " << (void*)vs.guest_FC3210 << std::endl;
	os << "EMNOTE: " << (void*)(intptr_t)vs.guest_EMNOTE << std::endl;
	os << "fs_base = " << (void*)vs.guest_FS_ZERO << std::endl;
}

#define FLAGS_MASK	(0xff | (1 << 10) | (1 << 11))
uint64_t AMD64CPUState::getRFLAGS(const VexGuestAMD64State& v)
{
	uint64_t guest_rflags = LibVEX_GuestAMD64_get_rflags(
		&const_cast<VexGuestAMD64State&>(v));
	guest_rflags &= FLAGS_MASK;
	guest_rflags |= (1 << 1);
	return guest_rflags;
}

void AMD64CPUState::print(std::ostream& os, const void* regctx) const
{
	VexGuestAMD64State	*s;
	s = const_cast<VexGuestAMD64State*>((const VexGuestAMD64State*)regctx);
	print(os, *s);
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

	assert (regs.fs_base != 0 && "TLS is important to have!!!");
	v.guest_FS_ZERO = regs.fs_base;

	/* XXX: in the future we want to slurp the full YMM registers */
	/* definitely need smarter ptrace code GETREGSET */
	for (unsigned i = 0; i < 16; i++) {
		void		*dst(&(((char*)&v.guest_YMM0)[i*32]));
		const void	*src(&(((const char*)&fpregs.xmm_space)[i*16]));
		memcpy(dst, src, 16);
	}


	//TODO: this is surely wrong, the sizes don't even match...
	memcpy(&v.guest_FPREG[0], &fpregs.st_space[0], sizeof(v.guest_FPREG));

	//TODO: floating point flags and extra fp state, sse  rounding
	//

	/* DF => string instructions auto-decrement */
	v.guest_DFLAG = (regs.eflags & (1 << 10)) ? -1 :1;
	v.guest_CC_OP = 0 /* AMD64G_CC_OP_COPY */;
	v.guest_CC_DEP1 = regs.eflags & (0xff | (3 << 10));
	v.guest_CC_DEP2 = 0;
	v.guest_CC_NDEP = v.guest_CC_DEP1;
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
