#include <errno.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <sys/syscall.h>
#include <sys/mman.h>

#include <sstream>

#include "syscall/syscallsmarshalled.h"
#include "ptimgchk.h"
#include "memlog.h"

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

#define get_reg_user(x,y)	*((uint64_t*)&x[user_regs_desc_tab[y].user_off]);
#define get_reg_vex(x,y)	*((uint64_t*)&x[user_regs_desc_tab[y].vex_off])
#define get_reg_name(y)		user_regs_desc_tab[y].name

/*
the basic concept here is to single step the subservient copy of the program
while the program counter is within a specific range.  that range will
correspond to the original range of addresses contained within the translation
block.  this will allow things to work properly in cases where the exit from
the block at the IR level re-enters the block.  this is nicer than the
alternative of injecting breakpoint instructions into the original process
because it avoids the need for actually parsing the opcodes at this level of
the code.  it's bad from the perspective of performance because a long running
loop that could have been entirely contained within a translation block won't
be able to fully execute without repeated syscalls to grab the process state

a nice alternative style of doing this type of partial checking would be to run until the next system call.  this can happen extremely efficiently as the
ptrace api provides a simple call to stop at that point.  identifying the
errant code that causes a divergence would be tricker, but the caller of such
a utility could track all of the involved translation blocks and produce
a summary of the type of instructions included in the IR.  as long as the bug
was an instruction misimplementation this would probably catch it rapidly.
this approach might be more desirable if we have divergent behavior that
happens much later in the execution cycle.  unfortunately, there are probably
many other malloc, getpid, gettimeofday type calls that will cause divergence
early enough to cause the cross checking technique to fail before the speed of
the mechanism really made an impact in how far it can check.
*/

PTImgChk::PTImgChk(int argc, char* const argv[], char* const envp[])
: GuestPTImg(argc, argv, envp)
, steps(0), bp_steps(0), blocks(0)
, hit_syscall(false)
, mem_log(getenv("VEXLLVM_LAST_STORE") ? new MemLog() : NULL)
{
	const char	*step_gauge;

	log_steps = (getenv("VEXLLVM_LOG_STEPS")) ? true : false;
	step_gauge =  getenv("VEXLLVM_STEP_GAUGE");
	if (step_gauge == NULL)
		log_gauge_overflow = 0;
	else {
		log_gauge_overflow = atoi(step_gauge);
		fprintf(stderr,
			"STEPS BETWEEN UPDATE: %d.\n",
			log_gauge_overflow);
	}
}

PTImgChk::~PTImgChk()
{
	if (mem_log) delete mem_log;
}

void PTImgChk::handleChild(pid_t pid)
{
	/* keep child around */
	child_pid = pid;
	//we need the fds to line up.  child will have only 0-4
	//TODO: actually check that
#if 1
	for(int i = 3; i < 1024; ++i)
		close(i);
#endif
}

void PTImgChk::stepThroughBounds(
	guest_ptr start, guest_ptr end,
	const VexGuestAMD64State& state)
{
	guest_ptr	new_ip;

	/* TODO: timeout? */
	do {
		new_ip = continueForwardWithBounds(start, end, state);
		if (hit_syscall)
			break;
	} while (new_ip >= start && new_ip < end);
}

guest_ptr PTImgChk::continueForwardWithBounds(
	guest_ptr start, guest_ptr end,
	const VexGuestAMD64State& state)
{
	user_regs_struct	regs;

	if (log_steps) {
		std::cerr << "RANGE: "
			<< (void*)start.o << "-" << (void*)end.o
			<< std::endl;
	}

	hit_syscall = false;
	while (doStep(start, end, regs, state)) {
		/* so we trap on backjumps */
		if (regs.rip > start)
			start = guest_ptr(regs.rip);
	}

	blocks++;
	return guest_ptr(regs.rip);
}

bool PTImgChk::isRegMismatch(
	const VexGuestAMD64State& state,
	const user_regs_struct& regs) const
{
	uint8_t		*user_regs_ctx;
	uint8_t		*vex_reg_ctx;

	vex_reg_ctx = (uint8_t*)&state;
	user_regs_ctx = (uint8_t*)&regs;

	for (unsigned int i = 0; i < GPR_COUNT; i++) {
		uint64_t	ureg, vreg;

		ureg = get_reg_user(user_regs_ctx, i);
		vreg = get_reg_vex(vex_reg_ctx, i);
		if (ureg != vreg) return true;
	}

	return false;
}

/* if we find an opcode that is causing problems, we'll replace the vexllvm
 * state with the pt state (since the pt state is by definition correct)
 */
bool PTImgChk::fixup(const guest_ptr ip_begin, const guest_ptr ip_end)
{
	guest_ptr cur_window, last_window;

	/* guest ip's are mapped in our addr space, no need for IPC */
	cur_window = ip_begin;
	last_window = guest_ptr(ip_end.o - (sizeof(long) - 1));
	while (cur_window < ip_end) {
		long		op;
		uint16_t	op16;

		mem->memcpy(&op, cur_window, sizeof(long));
		op16 = op & 0xffff;
		switch (op16) {
		case 0xbc0f: /* BSF */
		case 0xbd0f: /* BSR */
			fprintf(stderr,
				"[VEXLLVM] fixing up op=%p@IP=%p\n",
				(void*)op16,
				(void*)cur_window.o);
			slurpRegisters(child_pid);
			return true;
		default:
			break;
		}

		cur_window.o++;
	}

	fprintf(stderr, "VAIN ATTEMPT TO FIXUP %p-%p\n", 
		(void*)ip_begin.o, (void*)ip_end.o);
	/* couldn't figure out how to fix */
	return false;
}

static inline bool ldeqd(void* ld, long d) {
	long double* real = (long double*)ld;
	union {
		double d;
		long l;
	} alias;
	alias.d = *real;
	return alias.l == d;
}
static inline bool fcompare(unsigned int* a, long d) {
	return (*(long*)&d == *(long*)&a[0] &&
		(a[2] == 0 || a[2] == 0xFFFF) &&
		a[3] == 0) ||
		ldeqd(&a[0], d);
}
bool PTImgChk::isMatch(const VexGuestAMD64State& state) const
{
	user_regs_struct	regs;
	user_fpregs_struct	fpregs;
	int			err;
	bool			x86_fail, sse_ok, seg_fail, x87_ok, mem_ok;

	getRegs(regs);

	err = ptrace(PTRACE_GETFPREGS, child_pid, NULL, &fpregs);
	assert (err != -1 && "couldn't PTRACE_GETFPREGS");

	x86_fail = isRegMismatch(state, regs);
	seg_fail = (regs.fs_base ^ state.guest_FS_ZERO);

	//TODO: consider evaluating CC fields, ACFLAG, etc?
	//TODO: segments? shouldn't pop up on user progs..
	//TODO: what is this for? well besides the obvious
	// /* 192 */ULong guest_SSEROUND;

	sse_ok = !memcmp(
		&state.guest_XMM0,
		&fpregs.xmm_space[0],
		sizeof(fpregs.xmm_space));

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

	mem_ok = isMatchMemLog();

	return !x86_fail && x87_ok & sse_ok && !seg_fail && mem_ok;
}

bool PTImgChk::isMatchMemLog() const
{
	if (!mem_log) return true;

	char data[MemLog::MAX_STORE_SIZE / 8 + sizeof(long)];
	uintptr_t base = (uintptr_t)mem_log->getAddress();
	uintptr_t aligned = base & ~(sizeof(long) - 1);
	uintptr_t end = base + mem_log->getSize();
	unsigned int extra = base - aligned;

	for(long* mem = (long*)&data[0];
		aligned < end; aligned += sizeof(long), ++mem) {
		*mem = ptrace(PTRACE_PEEKTEXT, child_pid, aligned, NULL);
	}

	return memcmp(&data[extra], (char*)base, mem_log->getSize()) == 0;
}


bool PTImgChk::doStep(
	guest_ptr start, guest_ptr end,
	user_regs_struct& regs,
	const VexGuestAMD64State& state)
{
	getRegs(regs);

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

	if (isOnSysCall(regs)) {
		/* break on syscall */
		return false;
	}

	if (isOnRDTSC(regs)) {
		/* fake rdtsc to match vexhelpers.. */
		regs.rip += 2;
		regs.rax = 1;
		regs.rdx = 0;
		setRegs(regs);
		return true;
	}

	if (isPushF(regs)) {
		/* patch out the single step flag for perfect matching..
		   other flags (IF, reserved bit 1) need vex patch */
		waitForSingleStep();
		long old_v = ptrace(PTRACE_PEEKTEXT, child_pid, regs.rsp - sizeof(long), NULL);
		long new_v = old_v & ~0x100;
		long err = ptrace(PTRACE_POKETEXT, child_pid, regs.rsp - sizeof(long), new_v);
		assert (err != -1 && "Failed to patch pushed flags");
		return true;
	}

	if (isOnCPUID(regs)) {
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


long PTImgChk::getInsOp(long rip)
{
	if ((uintptr_t)rip == chk_addr) return chk_opcode;

	chk_addr = (uintptr_t)rip;
// SLOW WAY:
// Don't need to do this so long as we have the data at chk_addr in the guest
// process also mapped into the parent process at chk_addr.
//	chk_opcode = ptrace(PTRACE_PEEKTEXT, child_pid, regs.rip, NULL);

// FAST WAY: read it off like a boss
	chk_opcode = *((const long*)chk_addr);

	return chk_opcode;
}

long PTImgChk::getInsOp(const user_regs_struct& regs)
{
	return getInsOp(regs.rip);
}

#define OPCODE_SYSCALL	0x050f

bool PTImgChk::isOnSysCall(const user_regs_struct& regs)
{
	long	cur_opcode;
	bool	is_chk_addr_syscall;

	cur_opcode = getInsOp(regs);

	is_chk_addr_syscall = ((cur_opcode & 0xffff) == OPCODE_SYSCALL);
	hit_syscall |= is_chk_addr_syscall;

	return is_chk_addr_syscall;
}

bool PTImgChk::isOnRDTSC(const user_regs_struct& regs)
{
	long	cur_opcode;
	cur_opcode = getInsOp(regs);
	return (cur_opcode & 0xffff) == 0x310f;
}

bool PTImgChk::isOnCPUID(const user_regs_struct& regs)
{
	long	cur_opcode;
	cur_opcode = getInsOp(regs);
	return (cur_opcode & 0xffff) == 0xA20F;
}

bool PTImgChk::isPushF(const user_regs_struct& regs)
{
	long	cur_opcode;
	cur_opcode = getInsOp(regs);
	return (cur_opcode & 0xff) == 0x9C;
}


bool PTImgChk::filterSysCall(
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

void PTImgChk::stepSysCall(
	SyscallsMarshalled* sc_m, const VexGuestAMD64State& state)
{
	user_regs_struct	regs;
	long			old_rdi, old_r10;
	bool			syscall_restore_rdi_r10;
	int			sys_nr;

	getRegs(regs);

	/* do special syscallhandling if we're on an opcode */
	assert (isOnSysCall(regs));

	syscall_restore_rdi_r10 = false;
	old_rdi = regs.rdi;
	old_r10 = regs.r10;

	if (filterSysCall(state, regs)) {
		regs.rip += 2;
		//kernel clobbers these, assuming that the generated code, causes
		regs.rcx = regs.r11 = 0;
		setRegs(regs);
		return;
	}

	sys_nr = regs.rax;
	if (sys_nr == SYS_mmap || sys_nr == SYS_brk) {
		syscall_restore_rdi_r10 = true;
	}

	waitForSingleStep();

	getRegs(regs);
	if (syscall_restore_rdi_r10) {
		regs.r10 = old_r10;
		regs.rdi = old_rdi;
	}

	//kernel clobbers these, assuming that the generated code, causes
	regs.rcx = regs.r11 = 0;
	setRegs(regs);

	/* fixup any calls that affect memory */
	if (sc_m->isSyscallMarshalled(sys_nr)) {
		SyscallPtrBuf	*spb = sc_m->takePtrBuf();
		copyIn(spb->getPtr(), spb->getData(), spb->getLength());
		delete spb;
	}
}
uintptr_t PTImgChk::getSysCallResult() const {
	user_regs_struct	regs;
	getRegs(regs);
	return regs.rax;
}


void PTImgChk::setRegs(const user_regs_struct& regs)
{
	int	err;

	err = ptrace(PTRACE_SETREGS, child_pid, NULL, &regs);
	if(err < 0) {
		perror("PTImgChk::setregs");
		exit(1);
	}
}

void PTImgChk::copyIn(guest_ptr dst, const void* src, unsigned int bytes)
{
	char*		in_addr;
	guest_ptr	out_addr, end_addr;

	assert ((bytes % sizeof(long)) == 0);

	in_addr = (char*)src;
	out_addr = dst;
	end_addr = out_addr + bytes;

	while (out_addr < end_addr) {
		long data = *(long*)in_addr;
		int err = ptrace(PTRACE_POKEDATA, child_pid, 
			out_addr.o, data);
		assert(err == 0);
		in_addr += sizeof(long);
		out_addr.o += sizeof(long);
	}
}

guest_ptr PTImgChk::stepToBreakpoint(void)
{
	int	err, status;

	bp_steps++;
	err = ptrace(PTRACE_CONT, child_pid, NULL, NULL);
	if(err < 0) {
		perror("PTImgChk::doStep ptrace single step");
		exit(1);
	}
	wait(&status);

	if (!(WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP)) {
		user_regs_struct regs;
		fprintf(stderr,
			"OOPS. status: stopped=%d sig=%d status=%p\n",
			WIFSTOPPED(status), WSTOPSIG(status), (void*)status);
		getRegs(regs);
		stackTraceSubservient(std::cerr, guest_ptr(0),  guest_ptr(0));
		assert (0 == 1 && "bad wait from breakpoint");
	}

	/* ptrace executes trap, so child process's IP needs to be fixed */
	return undoBreakpoint(child_pid);
}

void PTImgChk::waitForSingleStep(void)
{
	int	err, status;

	steps++;
	err = ptrace(PTRACE_SINGLESTEP, child_pid, NULL, NULL);
	if(err < 0) {
		perror("PTImgChk::doStep ptrace single step");
		exit(1);
	}
	wait(&status);

	//TODO: real signal handling needed, but the main process
	//doesn't really have that yet...
	// 1407
	assert(	WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP &&
		"child received a signal or ptrace can't single step");

	if (log_gauge_overflow && (steps % log_gauge_overflow) == 0) {
		char	c = "/-\\|/-\\|"[(steps / log_gauge_overflow)%8];
		fprintf(stderr, "STEPS %09"PRIu64" %c %09"PRIu64" BLOCKS\r",
			steps, c, blocks);
	}
}

void PTImgChk::stackTraceSubservient(std::ostream& os, guest_ptr begin,
	guest_ptr end)
{
	stackTrace(os, getBinaryPath(), child_pid, begin, end);
}

void PTImgChk::printUserRegs(
	std::ostream& os,
	user_regs_struct& regs,
	const VexGuestAMD64State& ref) const
{
	uint8_t		*user_regs_ctx;
	uint8_t		*vex_reg_ctx;

	user_regs_ctx = (uint8_t*)&regs;
	vex_reg_ctx = (uint8_t*)&ref;
	for (unsigned int i = 0; i < REG_COUNT; i++) {
		uint64_t	user_reg, vex_reg;

		user_reg = get_reg_user(user_regs_ctx, i);
		vex_reg = get_reg_vex(vex_reg_ctx, i);
		if (user_reg != vex_reg) os << "***";

		os	<< user_regs_desc_tab[i].name << ": "
			<< (void*)user_reg << std::endl;
	}
}


void PTImgChk::printFPRegs(
	std::ostream& os,
	user_fpregs_struct& fpregs,
	const VexGuestAMD64State& ref) const
{
	//TODO: some kind of eflags, checking but i don't yet understand this
	//mess of broken apart state.

	//TODO: what is this for? well besides the obvious
	// /* 192 */ULong guest_SSEROUND;

	for(int i = 0; i < 16; ++i) {
		if (memcmp(
			&fpregs.xmm_space[i * 4],
			&(&ref.guest_XMM0)[i],
			sizeof(ref.guest_XMM0)))
		{
			os << "***";
		}

		os	<< "xmm" << i << ": "
			<< *((void**)&fpregs.xmm_space[i*4+2]) << "|"
			<< *((void**)&fpregs.xmm_space[i*4+0]) << std::endl;
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
			<< *(void**)&fpregs.st_space[i * 4 + 2] << "|"
			<< *(void**)&fpregs.st_space[i * 4 + 0] << std::endl;
	}
}

void PTImgChk::printMemory(std::ostream& os) const
{
	if (!mem_log) return;

	if(!isMatchMemLog())
		os << "***";
	os << "last write @ " << (void*)mem_log->getAddress().o
		<< " size: " << mem_log->getSize();

	char data[MemLog::MAX_STORE_SIZE / 8 + sizeof(long)];
	memset(&data[0], 0, sizeof(data));
	uintptr_t base = (uintptr_t)mem_log->getAddress();
	uintptr_t aligned = base & ~(sizeof(long) - 1);
	uintptr_t end = base + mem_log->getSize();
	unsigned int extra = base - aligned;
	for(long* lmem = (long*)&data[0];
		aligned < end; aligned += sizeof(long), ++lmem) {
		*lmem = ptrace(PTRACE_PEEKTEXT, child_pid,
			aligned, NULL);
	}
	memmove(&data[0], &data[extra], mem_log->getSize());
	os << " data: " << *(void**)&data[sizeof(void*)] << "|"
		<< *(void**)&data[0];

	void* vexdata[2] = {0, 0};
	mem->memcpy(&vexdata[0], mem_log->getAddress(),
		mem_log->getSize());
	os << " vexdata: " << vexdata[1] << "|" << vexdata[0];

	memcpy(&vexdata[0], mem_log->getData(),
		mem_log->getSize());
	os << " logged: " << vexdata[1] << "|" << vexdata[0];

	os << std::endl;
}

void PTImgChk::getRegs(user_regs_struct& regs) const
{
	int	err;

	err = ptrace(PTRACE_GETREGS, child_pid, NULL, &regs);
	if (err >= 0) return;

	perror("PTImgChk::getRegs");

	/* The politics of failure have failed.
	 * It is time to make them work again.
	 * (this is temporary nonsense to get
	 *  /usr/bin/make xchk tests not to hang). */
	waitpid(child_pid, NULL, 0);
	kill(child_pid, SIGABRT);
	raise(SIGKILL);
	_exit(-1);
	abort();
}

void PTImgChk::printSubservient(
	std::ostream& os, const VexGuestAMD64State& ref) const
{
	user_regs_struct	regs;
	user_fpregs_struct	fpregs;
	int			err;

	getRegs(regs);

	err = ptrace(PTRACE_GETFPREGS, child_pid, NULL, &fpregs);
	if(err < 0) {
		perror("PTImgChk::printState ptrace get fp registers");
		exit(1);
	}

	printUserRegs(os, regs, ref);
	printFPRegs(os, fpregs, ref);
	printMemory(os);
}

void PTImgChk::printTraceStats(std::ostream& os)
{
	os	<< "Traced "
		<< blocks << " blocks, stepped "
		<< steps << " instructions" << std::endl;
}

bool PTImgChk::breakpointSysCalls(const guest_ptr ip_begin, 
	const guest_ptr ip_end)
{
	guest_ptr	rip = ip_begin;
	bool	set_bp = false;

	while (rip != ip_end) {
		if (((getInsOp(rip) & 0xffff) == 0x050f)) {
			setBreakpoint(rip);
			set_bp = true;
		}
		rip.o++;
	}

	return set_bp;
}
