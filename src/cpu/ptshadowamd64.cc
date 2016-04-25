#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <stddef.h>
#include <signal.h>

#include "cpu/ptshadowamd64.h"
#include "cpu/ptamd64cpustate.h"
#include "cpu/amd64cpustate.h"

#define _pt_cpu	((PTAMD64CPUState*)pt_cpu.get())

extern "C" {
extern void amd64g_dirtyhelper_CPUID_baseline ( VexGuestAMD64State* st );
}

struct user_regs_desc
{
	const char*	name;
	int		user_off;
	int		vex_off;
};

#define USERREG_ENTRY(x,y)	{ 		\
	#x,					\
	offsetof(struct user_regs_struct, x),	\
	offsetof(VexGuestAMD64State, guest_##y) }


#define REG_COUNT	18
#define GPR_COUNT	16
const struct user_regs_desc user_regs_desc_tab[REG_COUNT] =
{
	USERREG_ENTRY(rip, RIP),
	USERREG_ENTRY(rax, RAX),
	USERREG_ENTRY(rbx, RBX),
	USERREG_ENTRY(rcx, RCX),
	USERREG_ENTRY(rdx, RDX),
	USERREG_ENTRY(rsp, RSP),
	USERREG_ENTRY(rbp, RBP),
	USERREG_ENTRY(rdi, RDI),
	USERREG_ENTRY(rsi, RSI),
	USERREG_ENTRY(r8, R8),
	USERREG_ENTRY(r9, R9),
	USERREG_ENTRY(r10, R10),
	USERREG_ENTRY(r11, R11),
	USERREG_ENTRY(r12, R12),
	USERREG_ENTRY(r13, R13),
	USERREG_ENTRY(r14, R14),
	USERREG_ENTRY(r15, R15),
	USERREG_ENTRY(fs_base, FS_CONST)
	// TODO: segments?
	// but valgrind/vex seems to not really fully handle them, how sneaky
};

#define get_reg_user(x,y)	\
	*((const uint64_t*)&x[user_regs_desc_tab[y].user_off])
#define set_reg_user(x,y,z)	\
	*((uint64_t*)&x[user_regs_desc_tab[y].user_off]) = z
#define get_reg_vex(x,y)	\
	*((const uint64_t*)&x[user_regs_desc_tab[y].vex_off])
#define get_reg_name(y)		user_regs_desc_tab[y].name

#define FLAGS_MASK	(0xff | (1 << 10) | (1 << 11))

#define _pt_cpu	((PTAMD64CPUState*)pt_cpu.get())

static inline bool ldeqd(void* ld, long d)
{
	long double* real = (long double*)ld;
	union {
		double d;
		long l;
	} alias;
	alias.d = *real;
	return alias.l == d;
}

static inline bool fcompare(unsigned int* a, long d)
{
	return (*(long*)&d == *(long*)&a[0] &&
		(a[2] == 0 || a[2] == 0xFFFF) &&
		a[3] == 0) ||
		ldeqd(&a[0], d);
}

PTShadowAMD64::PTShadowAMD64(GuestPTImg* gs, int in_pid)
: PTImgAMD64(gs, in_pid)
, xchk_eflags(getenv("VEXLLVM_XCHK_EFLAGS") ? true : false)
, fixup_eflags(getenv("VEXLLVM_NO_EFLAGS_FIXUP") ? false : true)
{}

bool PTShadowAMD64::isRegMismatch(void) const
{
	auto vex_reg_ctx = (const uint8_t*)&getVexState();
	auto user_regs_ctx = (const uint8_t*)&_pt_cpu->getRegs();

	for (unsigned int i = 0; i < GPR_COUNT; i++) {
		uint64_t ureg = get_reg_user(user_regs_ctx, i);
		uint64_t vreg = get_reg_vex(vex_reg_ctx, i);
		if (ureg != vreg) return true;
	}

	return false;
}

bool PTShadowAMD64::isMatch(void) const
{
	const user_regs_struct	*regs;
	user_fpregs_struct	fpregs;
	int			err;
	bool			x86_fail, sse_ok, seg_fail, x87_ok;
	const VexGuestAMD64State& state(getVexState());

	regs = &_pt_cpu->getRegs();

	err = ptrace(PTRACE_GETFPREGS, child_pid, NULL, &fpregs);
	assert (err != -1 && "couldn't PTRACE_GETFPREGS");

	x86_fail = isRegMismatch();

	// if ptrace fs_base == 0, then we have a fixup but the ptraced
	// process doesn't-- don't flag as an error!
	seg_fail = (regs->fs_base != 0)
		? (regs->fs_base ^ state.guest_FS_CONST)
		: false;

	//TODO: consider evaluating CC fields, ACFLAG, etc?
	//TODO: segments? shouldn't pop up on user progs..
	//TODO: what is this for? well besides the obvious
	// /* 192 */ULong guest_SSEROUND;
	if (xchk_eflags) {
		uint64_t guest_rflags, eflags;
		eflags = regs->eflags;
		eflags &= FLAGS_MASK;
		guest_rflags = AMD64CPUState::getRFLAGS(state);
		if (eflags != guest_rflags)
			return false;
	}

	/* XXX: PARTIAL. DOES NOT CONSIDER FULL YMM REGISTER */
	for (unsigned i = 0; i < 16; i++) {
		sse_ok = !memcmp(
			((const char*)&state.guest_YMM0) + i*32,
			((const char*)&fpregs.xmm_space) + i*16,
			16);
		if (!sse_ok) break;
	}

	//TODO: check the top pointer of the floating point stack..
	// /* FPU */
	// /* 456 */UInt  guest_FTOP;
	//FPTAG?

	//what happens if the FP unit is doing long doubles?
	//if VEX supports this, it probably should be filling in
	//the extra 16 bits with the appropriate thing on MMX
	//operations, like a real x86 cpu
	x87_ok = true;
	for(int i = 0; i < 8; ++i) {
		int r  = (state.guest_FTOP + i) & 0x7;
		bool is_ok = fcompare(
			&fpregs.st_space[2 * i],
			state.guest_FPREG[r]);
		if(!is_ok) {
			x87_ok = false;
			break;
		}
	}

	//TODO: what are these?
	// /* 536 */ ULong guest_FPROUND;
	// /* 544 */ ULong guest_FC3210;

	//TODO: more stuff that is likely unneeded
	// other vex internal state (tistart, nraddr, etc)

	return 	!x86_fail && x87_ok & sse_ok &&	!seg_fail;
}

#define MAX_INS_BUF	32
#define EFLAG_FIXUP(x)			\
	reason = x;			\
	if (!fixup_eflags) break;	\
	goto do_fixup;

PTShadow::FixupDir PTShadowAMD64::canFixup(
	const std::vector<InstExtent>& insts,
	bool has_memlog) const
{
	uint64_t	fix_op;
	guest_ptr	fix_addr;
	uint8_t		ins_buf[MAX_INS_BUF];
	const char	*reason = NULL;
	int		cur_s = wss_status;

	/* detected an illegal instruction-- an instruction
	 * that the host didn't understand? */
	if (WSTOPSIG(cur_s) == SIGILL)
		return PTShadow::FIXUP_NATIVE;

	foreach (it, insts.begin(), insts.end()) {
		const InstExtent	&inst(*it);
		uint8_t			op8;
		uint16_t		op16;
		uint32_t		op32;
		int			off;

		assert (inst.second < MAX_INS_BUF);
		/* guest ip's are mapped in our addr space, no need for IPC */
		gs->getMem()->memcpy(ins_buf, inst.first, inst.second);
		fix_addr = inst.first;

		/* filter out fucking prefix byte */
		off = ( ins_buf[0] == 0x41 ||
			ins_buf[0] == 0x44 ||
			ins_buf[0] == 0x48 ||
			ins_buf[0] == 0x49) ? 1 : 0;
		op8 = ins_buf[off];
		fix_op = op8;
		switch (op8) {
		case 0xd3: /* shl    %cl,%rsi */
		case 0xc1: EFLAG_FIXUP("shl");
		case 0xf7: EFLAG_FIXUP("idiv");
		case 0x69: /* IMUL */
		case 0x6b: EFLAG_FIXUP("imul");
		default:
			break;
		}


		op16 = ins_buf[off] | (ins_buf[off+1] << 8);
		fix_op = op16;
		switch (op16) {
		case 0x6b4c:
		case 0xaf0f: /* imul   0x24(%r14),%edx */
			EFLAG_FIXUP("imul");
		case 0xc06b:
		case 0xa30f: /* BT */
			EFLAG_FIXUP("bt");
		case 0xbc0f: reason = "bsf"; goto do_fixup;
		case 0xbd0f: reason = "bsr"; goto do_fixup;
		#ifndef BROKEN_OSDI
		case 0xae0f: /* mxcsr registers */
			reason = "mxcsr"; goto do_fixup;
		#endif
		default:
			break;
		}

		/* kind of slow / stupid, but oh well */
		for (unsigned i = 0; i < inst.second; i++) {
			op32 =	ins_buf[i] << 0 | (ins_buf[i+1] << 8) |
				(ins_buf[i+2] << 16) | (ins_buf[i+3] << 24);
			fix_op = op32;
			switch (op32) {
		#ifndef BROKEN_OSDI
			case 0xc05e0ff2:
				/* so LLVM will translate this to use 0x7ff80...
				 * when xmm0 is known to be zero when hardware
				 * generate 0xfff80... */
				reason = "divsd %xmm0, %xmm0";
				goto do_fixup;
		#endif
			/* accessing kernel timer page */
			case 0xff5ff080:
				reason = "readtimer-80";
				goto do_fixup;
			case 0xff5ff0b0:
				reason = "readtimer-b0";
				goto do_fixup;
			case 0xff5ff0a8:
				reason = "syspage-syslog?";
				goto do_fixup;
			}
		}

		if (has_memlog) {
			switch(op16) {
			case 0xa30f: /* fucking BT writes to stack! */
				fix_op = op16;
				reason = "bt-stack";
				goto do_fixup;
			default:
				break;
			}
		}
	}

	/* couldn't figure out how to fix */
	return PTShadow::FIXUP_NONE;

do_fixup:
	fprintf(stderr,
		"[VEXLLVM] fixing up op=%p@IP=%p. Reason=%s\n",
		(void*)fix_op,
		(void*)fix_addr.o,
		reason);
	return PTShadow::FIXUP_GUEST;
}

void PTShadowAMD64::printFPRegs(std::ostream& os) const
{
	int			err;
	user_fpregs_struct	fpregs;
	const VexGuestAMD64State& ref(getVexState());


	err = ptrace(PTRACE_GETFPREGS, child_pid, NULL, &fpregs);
	assert (err != -1 && "couldn't PTRACE_GETFPREGS");

	//TODO: some kind of eflags, checking but i don't yet understand this
	//mess of broken apart state.

	//TODO: what is this for? well besides the obvious
	// /* 192 */ULong guest_SSEROUND;

/* 4 32-bit values per register */
#define XMM_TO_64(x,i,k) (void*)(x[i*4+k] | (((uint64_t)x[i*4+(k+1)]) << 32L))

	for(int i = 0; i < 16; ++i) {
		if (memcmp(
			(((const char*)&fpregs.xmm_space)) + i*16,
			((const char*)&ref.guest_YMM0) + i*32,
			16) != 0)
		{
			os << "***";
		}

		os	<< "xmm" << i << ": "
			<< XMM_TO_64(fpregs.xmm_space, i, 2)
			<< "|"
			<< XMM_TO_64(fpregs.xmm_space, i, 0)
			<< std::endl;
	}

	//TODO: check the top pointer of the floating point stack..
	// /* FPU */
	// /* 456 */UInt  guest_FTOP;
	//FPTAG?

	for(int i = 0; i < 8; ++i) {
		int r  = (ref.guest_FTOP + i) & 0x7;
		if (!fcompare(
			&fpregs.st_space[i * 2],
			ref.guest_FPREG[r]))
		{
			os << "***";
		}
		os << "st" << i << ": "
			<< XMM_TO_64(fpregs.st_space, i, 2) << "|"
			<< XMM_TO_64(fpregs.st_space, i, 0) << std::endl;
	}
}

void PTShadowAMD64::printUserRegs(std::ostream& os) const
{
	const uint8_t	*user_regs_ctx;
	const uint8_t	*vex_reg_ctx;
	uint64_t	guest_eflags;
	const VexGuestAMD64State& ref(getVexState());
	const user_regs_struct& regs(_pt_cpu->getRegs());

	user_regs_ctx = (const uint8_t*)&regs;
	vex_reg_ctx = (const uint8_t*)&ref;

	for (unsigned int i = 0; i < REG_COUNT; i++) {
		uint64_t	user_reg, vex_reg;

		user_reg = get_reg_user(user_regs_ctx, i);
		vex_reg = get_reg_vex(vex_reg_ctx, i);
		if (user_reg != vex_reg) os << "***";

		os	<< user_regs_desc_tab[i].name << ": "
			<< (void*)user_reg << std::endl;
	}

	guest_eflags = AMD64CPUState::getRFLAGS(ref);
	if ((regs.eflags & FLAGS_MASK) != guest_eflags)
		os << "***";
	os << "EFLAGS: " << (void*)(regs.eflags & FLAGS_MASK);
	if ((regs.eflags & FLAGS_MASK) != guest_eflags)
		os << " vs VEX:" << (void*)guest_eflags;
	os << '\n';
}

void PTShadowAMD64::ptrace2vex()
{
	const auto	&r(_pt_cpu->getRegs());
	const auto	&fp(_pt_cpu->getFPRegs());
	auto		&v(getVexState());
	ptrace2vex(r, fp, v);
}

void PTShadowAMD64::vex2ptrace(
	const VexGuestAMD64State& v,
	struct user_regs_struct& r,
	struct user_fpregs_struct& fp)
{
	/* zeroing out the regs struct causes KMC_PTRACE to stop working */
//	memset(&r, 0, sizeof(r));
	for (unsigned i = 0; i < REG_COUNT; i++) {
		set_reg_user(
			((uint8_t*)&r), i,
			get_reg_vex(((const uint8_t*)&v), i));
	}

	r.eflags = AMD64CPUState::getRFLAGS(v) | 0x200;

	/* XXX: still broken, doesn't handle YMM right */
	memset(&fp, 0, sizeof(fp));
	for (unsigned i = 0; i < 16; i++) {
		memcpy(	((uint8_t*)&fp.xmm_space) + i*16,
			((const uint8_t*)&v.guest_YMM0)+i*32,
			16);
	}
}

void PTShadowAMD64::ptrace2vex(
	const struct user_regs_struct& r,
	const struct user_fpregs_struct& fp,
	VexGuestAMD64State& v)
{
	AMD64CPUState::setRegs(v, r, fp);
}

void PTShadowAMD64::vex2ptrace(void)
{
	struct user_regs_struct		&r(_pt_cpu->getRegs());
	struct user_fpregs_struct	&fp(_pt_cpu->getFPRegs());
	const VexGuestAMD64State	&v(getVexState());
	vex2ptrace(v, r, fp);
}

void PTShadowAMD64::pushRegisters(void)
{
	vex2ptrace();
	_pt_cpu->setRegs(_pt_cpu->getRegs());
}

void PTShadowAMD64::handleBadProgress(void)
{
	guest_ptr	cur_pc(gs->getCPUState()->getPC());
	gs->getCPUState()->setPC(guest_ptr(0xbadbadbadbad));
	pushRegisters();
	gs->getCPUState()->setPC(cur_pc);
}


bool PTShadowAMD64::filterSysCall(void)
{
	auto &state = getVexState();
	auto &regs = _pt_cpu->getRegs();

	switch (regs.rax) {
	case SYS_brk:
		/* hint a better base for sbrk so that we can hang out
		   with out a rochambeau ensuing */
		if(regs.rdi == 0) {
			regs.rdi = 0x800000;
		}
		break;

	case SYS_exit_group:
		regs.rax = state.guest_RAX;
		return true;

	case SYS_getpid:
		regs.rax = getpid();
		return true;

	case SYS_mmap:
		regs.rdi = state.guest_RAX;
		regs.r10 |= MAP_FIXED;
		_pt_cpu->setRegs(regs);
		return false;
	}

	return false;
}

bool PTShadowAMD64::doStep(guest_ptr start, guest_ptr end, bool& hit_syscall)
{
	// XXX try ptimgamd64 version first
	struct user_regs_struct	&regs(_pt_cpu->getRegs());

	if (!doStepPreCheck(start, end, hit_syscall)) {
		return false;
	}

	if (isOnRDTSC()) {
		/* fake rdtsc to match vexhelpers.. */
		regs.rip += 2;
		regs.rax = 1;
		regs.rdx = 0;
		_pt_cpu->setRegs(regs);
		return true;
	}

	if (isPushF()) {
		/* patch out the single step flag for perfect matching..
		   other flags (IF, reserved bit 1) need vex patch */
		waitForSingleStep();

		long old_v, new_v, err;

		old_v = ptrace(
			PTRACE_PEEKDATA,
			child_pid,
			regs.rsp - sizeof(long),
			NULL);

		new_v = old_v & ~0x100;
		err = ptrace(
			PTRACE_POKEDATA,
			child_pid,
			regs.rsp - sizeof(long),
			new_v);
		assert (err != -1 && "Failed to patch pushed flags");
		return true;
	}

	if (isOnCPUID()) {
		/* fake cpuid to match vexhelpers */
		VexGuestAMD64State	fakeState;
		regs.rip += 2;
		fakeState.guest_RAX = regs.rax;
		fakeState.guest_RBX = regs.rbx;
		fakeState.guest_RCX = regs.rcx;
		fakeState.guest_RDX = regs.rdx;
		amd64g_dirtyhelper_CPUID_baseline(&fakeState);
		regs.rax = fakeState.guest_RAX;
		regs.rbx = fakeState.guest_RBX;
		regs.rcx = fakeState.guest_RCX;
		regs.rdx = fakeState.guest_RDX;
		_pt_cpu->setRegs(regs);
		return true;
	}

	waitForSingleStep();
	return true;
}

const VexGuestAMD64State& PTShadowAMD64::getVexState(void) const
{ return *((const VexGuestAMD64State*)gs->getCPUState()->getStateData()); }

VexGuestAMD64State& PTShadowAMD64::getVexState(void)
{ return *((VexGuestAMD64State*)gs->getCPUState()->getStateData()); }


extern "C" { extern void amd64_trampoline(void* x); }
void PTShadowAMD64::restore(void)
{
	VexGuestAMD64State& state(getVexState());
	state.guest_CC_NDEP = AMD64CPUState::getRFLAGS(state);
	amd64_trampoline(&state);
}

void PTShadowAMD64::slurpRegisters()
{
	PTImgAMD64::slurpRegisters();
	auto amd64_cpu_state = (AMD64CPUState*)gs->getCPUState();
	amd64_cpu_state->setRegs(_pt_cpu->getRegs(), _pt_cpu->getFPRegs());
}
