#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include "Sugar.h"
#include "guestcpustate.h"
#include "cpu/ptimgamd64.h"
#include "cpu/amd64cpustate.h"
#include "syscall/syscallsmarshalled.h"

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
	USERREG_ENTRY(fs_base, FS_ZERO)
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

static uint64_t get_rflags(const VexGuestAMD64State& state)
{
	uint64_t guest_rflags = LibVEX_GuestAMD64_get_rflags(
		&const_cast<VexGuestAMD64State&>(state));
	guest_rflags &= FLAGS_MASK;
	guest_rflags |= (1 << 1);
	return guest_rflags;
}

PTImgAMD64::PTImgAMD64(GuestPTImg* gs, int in_pid)
: PTImgArch(gs, in_pid)
, recent_shadow(false)
, xchk_eflags(getenv("VEXLLVM_XCHK_EFLAGS") ? true : false)
, fixup_eflags(getenv("VEXLLVM_NO_EFLAGS_FIXUP") ? false : true)
{}

bool PTImgAMD64::isRegMismatch(void) const
{
	const uint8_t *user_regs_ctx;
	const uint8_t *vex_reg_ctx;

	vex_reg_ctx = (const uint8_t*)&getVexState();
	user_regs_ctx = (const uint8_t*)&getRegs();

	for (unsigned int i = 0; i < GPR_COUNT; i++) {
		uint64_t	ureg, vreg;

		ureg = get_reg_user(user_regs_ctx, i);
		vreg = get_reg_vex(vex_reg_ctx, i);
		if (ureg != vreg) return true;
	}

	return false;
}

struct user_regs_struct& PTImgAMD64::getRegs(void) const
{
	int	err;

	if (recent_shadow) {
		return shadow_reg_cache;
	}

	err = ptrace(PTRACE_GETREGS, child_pid, NULL, &shadow_reg_cache);
	recent_shadow = true;
	if (err >= 0)
		return shadow_reg_cache;

	perror("PTImgAMD64::getRegs");

	/* The politics of failure have failed.
	 * It is time to make them work again.
	 * (this is temporary nonsense to get
	 *  /usr/bin/make xchk tests not to hang). */
	waitpid(child_pid, NULL, 0);
	kill(child_pid, SIGABRT);
	raise(SIGKILL);
	_exit(-1);
	abort();

	return shadow_reg_cache;
}

const VexGuestAMD64State& PTImgAMD64::getVexState(void) const
{ return *((const VexGuestAMD64State*)gs->getCPUState()->getStateData()); }

VexGuestAMD64State& PTImgAMD64::getVexState(void)
{ return *((VexGuestAMD64State*)gs->getCPUState()->getStateData()); }


bool PTImgAMD64::isMatch(void) const
{
	const user_regs_struct	*regs;
	user_fpregs_struct	fpregs;
	int			err;
	bool			x86_fail, sse_ok, seg_fail, x87_ok;
	const VexGuestAMD64State& state(getVexState());

	regs = &getRegs();

	err = ptrace(PTRACE_GETFPREGS, child_pid, NULL, &fpregs);
	assert (err != -1 && "couldn't PTRACE_GETFPREGS");

	x86_fail = isRegMismatch();

	// if ptrace fs_base == 0, then we have a fixup but the ptraced
	// process doesn't-- don't flag as an error!
	seg_fail = (regs->fs_base != 0)
		? (regs->fs_base ^ state.guest_FS_ZERO)
		: false;

	//TODO: consider evaluating CC fields, ACFLAG, etc?
	//TODO: segments? shouldn't pop up on user progs..
	//TODO: what is this for? well besides the obvious
	// /* 192 */ULong guest_SSEROUND;
	if (xchk_eflags) {
		uint64_t guest_rflags, eflags;
		eflags = regs->eflags;
		eflags &= FLAGS_MASK;
		guest_rflags = get_rflags(state);
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
			&fpregs.st_space[4 * i],
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

GuestPTImg::FixupDir PTImgAMD64::canFixup(
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
		return GuestPTImg::FIXUP_NATIVE;

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
		default:
			break;
		}

		/* kind of slow / stupid, but oh well */
		for (unsigned i = 0; i < inst.second; i++) {
			op32 =	ins_buf[i] << 0 | (ins_buf[i+1] << 8) |
				(ins_buf[i+2] << 16) | (ins_buf[i+3] << 24);
			fix_op = op32;
			switch (op32) {
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
	return GuestPTImg::FIXUP_NONE;

do_fixup:
	fprintf(stderr,
		"[VEXLLVM] fixing up op=%p@IP=%p. Reason=%s\n",
		(void*)fix_op,
		(void*)fix_addr.o,
		reason);
	return GuestPTImg::FIXUP_GUEST;
}

void PTImgAMD64::printFPRegs(std::ostream& os) const
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
			&fpregs.st_space[i * 4],
			ref.guest_FPREG[r]))
		{
			os << "***";
		}
		os << "st" << i << ": "
			<< XMM_TO_64(fpregs.st_space, i, 2) << "|"
			<< XMM_TO_64(fpregs.st_space, i, 0) << std::endl;
	}
}

uintptr_t PTImgAMD64::getSysCallResult() const { return getRegs().rax; }
guest_ptr PTImgAMD64::getPC(void) { return guest_ptr(getRegs().rip); }
guest_ptr PTImgAMD64::getStackPtr(void) const {return guest_ptr(getRegs().rsp);}


void PTImgAMD64::printUserRegs(std::ostream& os) const
{
	const uint8_t	*user_regs_ctx;
	const uint8_t	*vex_reg_ctx;
	uint64_t	guest_eflags;
	const VexGuestAMD64State& ref(getVexState());
	const user_regs_struct& regs(getRegs());

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

	guest_eflags = get_rflags(ref);
	if ((regs.eflags & FLAGS_MASK) != guest_eflags)
		os << "***";
	os << "EFLAGS: " << (void*)(regs.eflags & FLAGS_MASK);
	if ((regs.eflags & FLAGS_MASK) != guest_eflags)
		os << " vs VEX:" << (void*)guest_eflags;
	os << '\n';
}

long PTImgAMD64::getInsOp(void) { return getInsOp(getRegs().rip); }

long PTImgAMD64::getInsOp(long pc)
{
	if ((uintptr_t)pc== chk_addr.o)
		return chk_opcode;

	chk_addr = guest_ptr(pc);
// SLOW WAY:
// Don't need to do this so long as we have the data at chk_addr in the guest
// process also mapped into the parent process at chk_addr.
//	chk_opcode = ptrace(PTRACE_PEEKTEXT, child_pid, regs.rip, NULL);

// FAST WAY: read it off like a boss
	chk_opcode = *((const long*)gs->getMem()->getHostPtr(chk_addr));
	return chk_opcode;
}


#define OPCODE_SYSCALL	0x050f

bool PTImgAMD64::isOnSysCall()
{
	long	cur_opcode;
	bool	is_chk_addr_syscall;

	cur_opcode = getInsOp();
	is_chk_addr_syscall = ((cur_opcode & 0xffff) == OPCODE_SYSCALL);
	return is_chk_addr_syscall;
}

bool PTImgAMD64::isOnRDTSC()
{
	long	cur_opcode;
	cur_opcode = getInsOp();
	return (cur_opcode & 0xffff) == 0x310f;
}

bool PTImgAMD64::isOnCPUID()
{
	long	cur_opcode;
	cur_opcode = getInsOp();
	return (cur_opcode & 0xffff) == 0xA20F;
}

bool PTImgAMD64::isPushF()
{
	long	cur_opcode;
	cur_opcode = getInsOp();
	return (cur_opcode & 0xff) == 0x9C;
}

bool PTImgAMD64::doStep(guest_ptr start, guest_ptr end, bool& hit_syscall)
{
	struct user_regs_struct	&regs(getRegs());

	/* check rip before executing possibly out of bounds instruction*/
	if (regs.rip < start || regs.rip >= end) {
		if(log_steps) {
			std::cerr << "STOPPING: "
				<< (void*)regs.rip << " not in ["
				<< (void*)start.o << ", "
				<< (void*)end.o << "]"
				<< std::endl;
		}
		/* out of bounds, report back, no more stepping */
		return false;
	}

	/* instruction is in-bounds, run it */
	if (log_steps)
		std::cerr << "STEPPING: " << (void*)regs.rip << std::endl;

	if (isOnSysCall()) {
		/* break on syscall */
		hit_syscall = true;
		return false;
	}

	if (isOnRDTSC()) {
		/* fake rdtsc to match vexhelpers.. */
		regs.rip += 2;
		regs.rax = 1;
		regs.rdx = 0;
		setRegs(regs);
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
		setRegs(regs);
		return true;
	}

	waitForSingleStep();
	return true;
}

bool PTImgAMD64::filterSysCall(
	const VexGuestAMD64State& state,
	user_regs_struct& regs)
{
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
		setRegs(regs);
		return false;
	}

	return false;
}

void PTImgAMD64::ignoreSysCall(void)
{
	user_regs_struct	*regs;

	/* do special syscallhandling if we're on an opcode */
	assert (isOnSysCall());

	regs = &getRegs();
	regs->rip += 2;
	/* kernel clobbers these */
	regs->rcx = regs->r11 = 0;
	setRegs(*regs);
	return;
}

void PTImgAMD64::stepSysCall(SyscallsMarshalled* sc_m)
{
	user_regs_struct	*regs;
	long			old_rdi, old_r10;
	bool			syscall_restore_rdi_r10;
	int			sys_nr;

	regs = &getRegs();

	/* do special syscallhandling if we're on an opcode */
	assert (isOnSysCall());

	syscall_restore_rdi_r10 = false;
	old_rdi = regs->rdi;
	old_r10 = regs->r10;

	if (filterSysCall(getVexState(), *regs)) {
		ignoreSysCall();
		return;
	}

	sys_nr = regs->rax;
	if (sys_nr == SYS_mmap || sys_nr == SYS_brk) {
		syscall_restore_rdi_r10 = true;
	}

	waitForSingleStep();

	regs = &getRegs();
	if (syscall_restore_rdi_r10) {
		regs->r10 = old_r10;
		regs->rdi = old_rdi;
	}

	//kernel clobbers these, assuming that the generated code, causes
	regs->rcx = regs->r11 = 0;
	setRegs(*regs);

	/* fixup any calls that affect memory */
	if (sc_m->isSyscallMarshalled(sys_nr)) {
		SyscallPtrBuf	*spb = sc_m->takePtrBuf();
		copyIn(spb->getPtr(), spb->getData(), spb->getLength());
		delete spb;
	}
}

void PTImgAMD64::setRegs(const user_regs_struct& regs)
{
	int	err;

	err = ptrace(PTRACE_SETREGS, child_pid, NULL, &regs);
	if (err < 0) {
		fprintf(stderr, "DIDN'T TAKE?? pid=%d\n", child_pid);
		perror("PTImgAMD64::setregs");
		abort();
		exit(1);
	}

	if (&regs != &shadow_reg_cache) {
		memcpy(&shadow_reg_cache, &regs, sizeof(regs));
	}

	recent_shadow = true;
}

bool PTImgAMD64::breakpointSysCalls(
	const guest_ptr ip_begin,
	const guest_ptr ip_end)
{
	guest_ptr	rip = ip_begin;
	bool		set_bp = false;

	while (rip != ip_end) {
		if (((getInsOp(rip) & 0xffff) == 0x050f)) {
			gs->setBreakpointByPID(child_pid, rip);
			set_bp = true;
		}
		rip.o++;
	}

	return set_bp;
}

guest_ptr PTImgAMD64::undoBreakpoint(void)
{
	struct user_regs_struct	regs;
	int			err;

	/* should be halted on our trapcode. need to set rip prior to
	 * trapcode addr */
	err = ptrace(PTRACE_GETREGS, child_pid, NULL, &regs);
	assert (err != -1);

	regs.rip--; /* backtrack before int3 opcode */
	err = ptrace(PTRACE_SETREGS, child_pid, NULL, &regs);

	/* run again w/out reseting BP and you'll end up back here.. */
	return guest_ptr(regs.rip);
}

long PTImgAMD64::setBreakpoint(guest_ptr addr)
{
	uint64_t		old_v, new_v;
	int			err;

	old_v = ptrace(PTRACE_PEEKTEXT, child_pid, addr.o, NULL);
	new_v = old_v & ~0xff;
	new_v |= 0xcc;

	err = ptrace(PTRACE_POKETEXT, child_pid, addr.o, new_v);
	assert (err != -1 && "Failed to set breakpoint");

	return old_v;
}

void PTImgAMD64::ptrace2vex(
	const user_regs_struct& urs,
	const user_fpregs_struct& fpregs,
	VexGuestAMD64State& vs)
{ AMD64CPUState::setRegs(vs, urs, fpregs); }

void PTImgAMD64::vex2ptrace(
	const VexGuestAMD64State& v,
	struct user_regs_struct& r)
{
	for (unsigned i = 0; i < REG_COUNT; i++) {
		set_reg_user(
			((uint8_t*)&r), i,
			get_reg_vex(((const uint8_t*)&v), i));
	}
	
	r.eflags = get_rflags(v);

	/* XXX: XMM REGISTERS!!! */
}

void PTImgAMD64::pushRegisters(void)
{
	struct	user_regs_struct	&r(getRegs());
	const VexGuestAMD64State	&v(getVexState());

	vex2ptrace(v, r);
	setRegs(r);
}

void PTImgAMD64::slurpRegisters(void)
{
	AMD64CPUState			*amd64_cpu_state;
	int				err;
	struct user_regs_struct		regs;
	struct user_fpregs_struct	fpregs;

	err = ptrace(PTRACE_GETREGS, child_pid, NULL, &regs);
	assert(err != -1);
	err = ptrace(PTRACE_GETFPREGS, child_pid, NULL, &fpregs);
	assert(err != -1);

	amd64_cpu_state = (AMD64CPUState*)gs->getCPUState();


	/*** linux is busted, quirks ahead ***/

	/* this is kind of a stupid hack but I don't know an easier way
	 * to get orig_rax */
	if (regs.orig_rax != ~((uint64_t)0)) {
		regs.rax = regs.orig_rax;
		if (regs.fs_base == 0) regs.fs_base = 0xdead;
	} else if (regs.fs_base == 0) {
		/* if it's static, it'll probably be using
		 * some native register bullshit for the TLS and it'll read 0.
		 *
		 * Patch this transgression up by allocating some pages
		 * to do the work.
		 * (if N/A, the show goes on as normal)
		 */

		/* Yes, I tried ptrace/ARCH_GET_FS. NO DICE. */
		//err = ptrace(
		//	PTRACE_ARCH_PRCTL, pid, &regs.fs_base, ARCH_GET_FS);
		// fprintf(stderr, "%d %p\n",  err, regs.fs_base);

		int		res;
		guest_ptr	base_addr;

		/* I saw some negative offsets when grabbing errno,
		 * so allow for at least 4KB in either direction */
		res = gs->getMem()->mmap(
			base_addr,
			guest_ptr(0),
			4096*2,
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS,
			-1,
			0);
		assert (res == 0);
		regs.fs_base = base_addr.o + 4096;
	}
	amd64_cpu_state->setRegs(regs, fpregs);
}

void PTImgAMD64::waitForSingleStep(void)
{
	PTImgArch::waitForSingleStep();
	recent_shadow = false;
}

void PTImgAMD64::revokeRegs(void)
{ recent_shadow = false; }

void PTImgAMD64::getRegs(user_regs_struct& r) const
{ memcpy(&r, &getRegs(), sizeof(r)); }

void PTImgAMD64::resetBreakpoint(guest_ptr addr, long v)
{
	int err = ptrace(PTRACE_POKETEXT, child_pid, addr.o, v);
	assert (err != -1 && "Failed to reset breakpoint");
}

extern "C" { extern void amd64_trampoline(void* x); }
void PTImgAMD64::restore(void)
{
	VexGuestAMD64State& state(getVexState());

	state.guest_CC_NDEP = get_rflags(state);
	amd64_trampoline(&state);
}


uint64_t PTImgAMD64::dispatchSysCall(const SyscallParams& sp)
{
	struct user_regs_struct	old_regs, new_regs;
	uint64_t		ret;
	uint64_t		old_op;

	old_regs = getRegs();

	/* patch in system call opcode */
	old_op = ptrace(PTRACE_PEEKDATA, child_pid, old_regs.rip, NULL);
	ptrace(PTRACE_POKEDATA, child_pid, old_regs.rip, (void*)OPCODE_SYSCALL);

	new_regs = old_regs;
	new_regs.rax = sp.getSyscall();
	new_regs.rdi = sp.getArg(0);
	new_regs.rsi = sp.getArg(1);
	new_regs.rdx = sp.getArg(2);
	new_regs.r10 = sp.getArg(3);
	new_regs.r8 = sp.getArg(4);
	new_regs.r9 =  sp.getArg(5);
	ptrace(PTRACE_SETREGS, child_pid, NULL, &new_regs);

	waitForSingleStep();

	ret = getSysCallResult();

	/* reload old state */
	ptrace(PTRACE_POKEDATA, child_pid, old_regs.rip, (void*)old_op);
	setRegs(old_regs);

	return ret;
}
