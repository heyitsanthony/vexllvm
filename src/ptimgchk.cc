#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/mman.h>

#include <sstream>

#include "ptimgchk.h"

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

//the basic concept here is to single step the subservient copy of the program
//while the program counter is within a specific range.  that range will 
//correspond to the original range of addresses contained within the translation 
//block.  this will allow things to work properly in cases where the exit from 
//the block at the IR level re-enters the block.  this is nicer than the
//alternative of injecting breakpoint instructions into the original process 
//because it avoids the need for actually parsing the opcodes at this level of 
//the code.  it's bad from the perspective of performance because a long running 
//loop that could have been entirely contained within a translation block won't 
//be able to fully execute without repeated syscalls to grab the process state
//
//a nice alternative style of doing this type of partial checking would be to run 
//until the next system call.  this can happen extremely efficiently as the 
//ptrace api provides a simple call to stop at that point.  identifying the 
//errant code that causes a divergence would be tricker, but the caller of such 
//a utility could track all of the involved translation blocks and produce
//a summary of the type of instructions included in the IR.  as long as the bug 
//was an instruction misimplementation this would probably catch it rapidly.  
//this approach might be more desirable if we have divergent behavior that 
//happens much later in the execution cycle.  unfortunately, there are probably 
//many other malloc, getpid, gettimeofday type calls that will cause divergence 
//early enough to cause the cross checking technique to fail before the speed of
//the mechanism really made an impact in how far it can check.

PTImgChk::PTImgChk(int argc, char* const argv[], char* const envp[])
: 	GuestStatePTImg(argc, argv, envp),
	binary(argv[0]), steps(0), blocks(0)
{
	log_steps = (getenv("VEXLLVM_LOG_STEPS")) ? true : false;
}


void PTImgChk::handleChild(pid_t pid)
{
	/* keep child around */
	child_pid = pid;
}

bool PTImgChk::continueWithBounds(
	uint64_t start, uint64_t end, const VexGuestAMD64State& state)
{
	int			err;
	user_regs_struct	regs;
	user_fpregs_struct	fpregs;

	if (log_steps) {
		std::cerr << "RANGE: " 
			<< (void*)start << "-" << (void*)end
			<< std::endl;
	}

	while (doStep(start, end, regs, state));
	
	blocks++;
	err = ptrace(PTRACE_GETFPREGS, child_pid, NULL, &fpregs);
	if(err < 0) {
		perror("PTImgChk::continueWithBounds  ptrace get fp registers");
		exit(1);
	}
	
	return isGuestFailed(state, regs, fpregs);
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

bool PTImgChk::isGuestFailed(
	const VexGuestAMD64State& state,
	user_regs_struct& regs,
	user_fpregs_struct& fpregs) const
{
	bool x86_fail, sse_ok, seg_fail, x87_ok;

	x86_fail = isRegMismatch(state, regs);
	seg_fail = (regs.fs_base ^ state.guest_FS_ZERO);

	//TODO: some kind of eflags, checking but i don't yet understand this
	//mess of broken apart state.
	//
	// /* 4-word thunk used to calculate O S Z A C P flags. */
	// /* 128 */ ULong  guest_CC_OP;
	// /* 136 */ ULong  guest_CC_DEP1;
	// /* 144 */ ULong  guest_CC_DEP2;
	// /* 152 */ ULong  guest_CC_NDEP;
	// /* The D flag is stored here, encoded as either -1 or +1 */
	// /* 160 */ ULong  guest_DFLAG;
	// /* 168 */ ULong  guest_RIP;
	// /* Bit 18 (AC) of eflags stored here, as either 0 or 1. */
	// /* ... */ ULong  guest_ACFLAG;
	// /* Bit 21 (ID) of eflags stored here, as either 0 or 1. */
	
	//TODO: segments? but valgrind/vex seems to not really fully handle them, how sneaky

	//TODO: what is this for? well besides the obvious
	// /* 192 */ULong guest_SSEROUND;

	sse_ok = !memcmp(&state.guest_XMM0, &fpregs.xmm_space[0], sizeof(fpregs.xmm_space));

	//TODO: check the top pointer of the floating point stack..
	// /* FPU */
	// /* 456 */UInt  guest_FTOP;
	//FPTAG?

	//TODO: this is surely wrong, the sizes don't even match...
	x87_ok = !memcmp(&state.guest_FPREG[0], &fpregs.st_space[0], sizeof(state.guest_FPREG));

	//TODO: what are these?
	// /* 536 */ ULong guest_FPROUND;
	// /* 544 */ ULong guest_FC3210;

	//probably not TODO: more stuff that is likely unneeded
	// /* 552 */ UInt  guest_EMWARN;
	// ULong guest_TISTART;
	// ULong guest_TILEN;
	// ULong guest_NRADDR;
	// ULong guest_SC_CLASS;
	// ULong guest_GS_0x60;
	// ULong guest_IP_AT_SYSCALL;
	// ULong padding;

	return !x86_fail && x87_ok & sse_ok && !seg_fail;
}

bool PTImgChk::doStep(
	uint64_t start, uint64_t end,
	user_regs_struct& regs,
	const VexGuestAMD64State& state)
{
	long	old_rdi, old_r10;
	long	cur_opcode;
	bool	is_syscall;
	bool	syscall_restore_rdi_r10;
	int	err;

	err = ptrace(PTRACE_GETREGS, child_pid, NULL, &regs);
	if (err < 0) {
		perror("PTImgChk::doStep ptrace get registers");
		assert (0 == 1 && "KABOOM");
		exit(1);
	}

	if (regs.rip < start || regs.rip >= end) {
		if(log_steps) 
			std::cerr << "STOPPING: " 
				<< (void*)regs.rip << " not in ["
				<< (void*)start << ", "
				<< (void*)end << "]"
				<< std::endl;
		return false;
	} else {
		if(log_steps) 
			std::cerr << "STEPPING: "
				<< (void*)regs.rip << std::endl;
	}

 	cur_opcode = ptrace(PTRACE_PEEKTEXT, child_pid, regs.rip, NULL);
	is_syscall = (cur_opcode & 0xffff) == 0x050f;
	syscall_restore_rdi_r10 = false; 
	if (is_syscall) {
		old_rdi = regs.rdi;
		old_r10 = regs.r10;

		if (handleSysCall(state, regs))
			return true;
		
		if (regs.rax == SYS_mmap)
			syscall_restore_rdi_r10 = true;
	}

	singleStep();

	if (is_syscall) {
		err = ptrace(PTRACE_GETREGS, child_pid, NULL, &regs);
		if (err < 0) {
			perror("PTImgChk::continueWithBounds ptrace post syscall refresh registers");
			exit(1);
		}

		if (syscall_restore_rdi_r10) {
			regs.r10 = old_r10;
			regs.rdi = old_rdi;
		}

		//kernel clobbers these, assuming that the generated code, causes
		regs.rcx = regs.r11 = 0;
		err = ptrace(PTRACE_SETREGS, child_pid, NULL, &regs);
		if(err < 0) {
			perror("PTImgChk::continueWithBounds ptrace set registers after syscall");
			exit(1);
		}
	}

	return true;
}

void PTImgChk::singleStep(void)
{
	int status;

	steps++;
	err = ptrace(PTRACE_SINGLESTEP, child_pid, NULL, NULL);
	if(err < 0) {
		perror("PTImgChk::doStep ptrace single step");
		exit(1);
	}

	//note that in this scenario we don't care whether or not
	//a syscall was the source of the trap (two traps are produced
	//per syscall, pre+post).
	wait(&status);

	fprintf(stderr, "%d %d <<<<<<<<<<<<<<\n",
		WIFSTOPPED(status), WSTOPSIG(status));
}

bool PTImgChk::handleSysCall(
	const VexGuestAMD64State& state,
	user_regs_struct& regs)
{
	int	err;

	switch (regs.rax) {
	case SYS_brk:
		regs.rax = -1;
		regs.rip += 2;
		regs.rcx = 0;
		regs.r11 = 0;
		err = ptrace(PTRACE_SETREGS, child_pid, NULL, &regs);
		if(err < 0) {
			perror("PTImgChk::continueWithBounds "
				"ptrace set registers pre-brk");
			exit(1);
		}
		return true;

	case SYS_getpid:
		regs.rax = getpid();
		regs.rip += 2;
		regs.rcx = regs.r11 = 0;
		err = ptrace(PTRACE_SETREGS, child_pid, NULL, &regs);
		if(err < 0) {
			perror("PTImgChk::continueWithBounds "
				"ptrace set registers pre-getpid");
			exit(1);
		}
		return true;

	case SYS_mmap:
		regs.rdi = state.guest_RAX;
		regs.r10 |= MAP_FIXED;
		err = ptrace(PTRACE_SETREGS, child_pid, NULL, &regs);
		if(err < 0) {
			perror("PTImgChk::continueWithBounds "
				"ptrace set registers pre-mmap");
			exit(1);
		}

		return false;
	}

	return false;
}

void PTImgChk::stackTraceSubservient(std::ostream& os) 
{
	stackTrace(os, binary, child_pid);
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
	//
	// /* 4-word thunk used to calculate O S Z A C P flags. */
	// /* 128 */ ULong  guest_CC_OP;
	// /* 136 */ ULong  guest_CC_DEP1;
	// /* 144 */ ULong  guest_CC_DEP2;
	// /* 152 */ ULong  guest_CC_NDEP;
	// /* The D flag is stored here, encoded as either -1 or +1 */
	// /* 160 */ ULong  guest_DFLAG;
	// /* 168 */ ULong  guest_RIP;
	// /* Bit 18 (AC) of eflags stored here, as either 0 or 1. */
	// /* ... */ ULong  guest_ACFLAG;
	// /* Bit 21 (ID) of eflags stored here, as either 0 or 1. */
	
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
		if (memcmp(
			&fpregs.st_space[i * 4], 
			&ref.guest_FPREG[i],
			sizeof(ref.guest_FPREG[0])))
		{
			os << "***";
		}
		os << "st" << i << ": " 
			<< *(void**)&fpregs.st_space[i * 4 + 2] << "|"
			<< *(void**)&fpregs.st_space[i * 4 + 0] << std::endl;
	}
}


void PTImgChk::printSubservient(
	std::ostream& os,
	const VexGuestAMD64State& ref) const
{
	user_regs_struct	regs;
	user_fpregs_struct	fpregs;
	int			err;

	err = ptrace(PTRACE_GETREGS, child_pid, NULL, &regs);
	if(err < 0) {
		perror("PTImgChk::printState ptrace get registers");
		exit(1);
	}

	err = ptrace(PTRACE_GETFPREGS, child_pid, NULL, &fpregs);
	if(err < 0) {
		perror("PTImgChk::printState ptrace get fp registers");
		exit(1);
	}
	
	printUserRegs(os, regs, ref);
	printFPRegs(os, fpregs, ref);
}

void PTImgChk::printTraceStats(std::ostream& os) 
{
	os	<< "Traced " 
		<< blocks << " blocks, stepped " 
		<< steps << " instructions" << std::endl;
}
