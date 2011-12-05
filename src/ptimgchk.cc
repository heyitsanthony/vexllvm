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

#include "Sugar.h"
#include "syscall/syscallsmarshalled.h"
#include "ptimgchk.h"
#include "memlog.h"

/*
 * single step shadow program while counter is in specific range
 */
PTImgChk::PTImgChk(int argc, char* const argv[], char* const envp[])
: GuestPTImg(argc, argv, envp)
, steps(0)
, bp_steps(0)
, blocks(0)
, hit_syscall(false)
, mem_log(getenv("VEXLLVM_LAST_STORE") ? new MemLog() : NULL)
, xchk_stack(getenv("VEXLLVM_XCHK_STACK") ? true : false)
{
	const char	*step_gauge;

	log_steps = (getenv("VEXLLVM_LOG_STEPS")) ? true : false;
	step_gauge = getenv("VEXLLVM_STEP_GAUGE");
	if (step_gauge == NULL) {
		log_gauge_overflow = 0;
	} else {
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
	for (int i = 3; i < 1024; ++i)
		close(i);
}

void PTImgChk::stepThroughBounds(guest_ptr start, guest_ptr end)
{
	guest_ptr	new_ip;

	/* TODO: timeout? */
	do {
		new_ip = continueForwardWithBounds(start, end);
		if (hit_syscall)
			break;
	} while (new_ip >= start && new_ip < end);
}

guest_ptr PTImgChk::continueForwardWithBounds(guest_ptr start, guest_ptr end)
{
	if (log_steps) {
		std::cerr << "RANGE: "
			<< (void*)start.o << "-" << (void*)end.o
			<< std::endl;
	}

	hit_syscall = false;
	while (pt_arch->doStep(start, end, hit_syscall)) {
		guest_ptr	pc(pt_arch->getPC());
		/* so we trap on backjumps */
		if (pc > start)
			start = pc;
	}

	blocks++;
	return pt_arch->getPC();
}

/* if we find an opcode that is causing problems, we'll replace the vexllvm
 * state with the pt state (since the pt state is by definition correct)
 */
bool PTImgChk::fixup(const std::vector<InstExtent>& insts)
{
	bool		do_fixup;

	do_fixup = pt_arch->canFixup(insts, mem_log != NULL);
	if (do_fixup == false) {
		fprintf(stderr, "VAIN ATTEMPT TO FIXUP %p-%p\n",
			(void*)(insts.front().first.o),
			(void*)(insts.back().first.o));
		/* couldn't figure out how to fix */
		return false;
	}

	slurpRegisters(child_pid);
	if (mem_log) mem_log->clear();

	return true;
}

bool PTImgChk::isMatch() const
{
	assert (0 == 1 && "STUB");
}

#define MEMLOG_BUF_SZ	(MemLog::MAX_STORE_SIZE / 8 + sizeof(long))
void PTImgChk::readMemLogData(char* data) const
{
	uintptr_t	base, aligned, end;
	unsigned int	extra;

	memset(data, 0, MEMLOG_BUF_SZ);

	base = (uintptr_t)mem_log->getAddress();
	aligned = base & ~(sizeof(long) - 1);
	end = base + mem_log->getSize();
	extra = base - aligned;

	for (long* mem = (long*)&data[0];
		aligned < end;
		aligned += sizeof(long), ++mem)
	{
		*mem = ptrace(PTRACE_PEEKDATA, child_pid, aligned, NULL);
	}

	if (extra != 0)
		memmove(&data[0], &data[extra], mem_log->getSize());
}

bool PTImgChk::isMatchMemLog(void) const
{
	char	data[MEMLOG_BUF_SZ];
	int	cmp;

	if (!mem_log)
		return true;

	readMemLogData(data);

	cmp = memcmp(
		data,
		mem->getHostPtr(mem_log->getAddress()),
		mem_log->getSize());

	return (cmp == 0);
}

/* was seeing errors at 0x2f0 in cc1, so 0x400 seems like a good place */
#define STACK_XCHK_BYTES	(1024+128)
#define STACK_XCHK_LONGS	(int)(STACK_XCHK_BYTES/sizeof(long))

bool PTImgChk::isStackMatch(void) const
{
	guest_ptr	stack_base;

	if (!xchk_stack)
		return true;

	/* reg.rsp */
	stack_base = pt_arch->getStackPtr();

	/* -128 for amd64 redzone */
	for (int i = -128/((int)sizeof(long)); i < STACK_XCHK_LONGS; i++) {
		guest_ptr	stack_ptr(stack_base.o+sizeof(long)*i);
		long		pt_val, guest_val;

		pt_val = ptrace(PTRACE_PEEKDATA, child_pid, stack_ptr.o, NULL);

		guest_val = getMem()->read<long>(stack_ptr);
		if (pt_val == guest_val)
			continue;

		fprintf(stderr, "Stack mismatch@%p (pt=%p!=%p=vex)\n",
			(void*)stack_ptr.o,
			(void*)pt_val,
			(void*)guest_val);
		return false;
	}

	return true;
}

long PTImgChk::getInsOp(long pc)
{
	if ((uintptr_t)pc== chk_addr.o)
		return chk_opcode;

	chk_addr = guest_ptr(pc);
// SLOW WAY:
// Don't need to do this so long as we have the data at chk_addr in the guest
// process also mapped into the parent process at chk_addr.
//	chk_opcode = ptrace(PTRACE_PEEKTEXT, child_pid, regs.rip, NULL);

// FAST WAY: read it off like a boss
	chk_opcode = *((const long*)getMem()->getHostPtr(chk_addr));
	return chk_opcode;
}


uintptr_t PTImgChk::getSysCallResult(void) const
{ return pt_arch->getSysCallResult(); }

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
		int err = ptrace(PTRACE_POKEDATA, child_pid, out_addr.o, data);
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
		fprintf(stderr,
			"OOPS. status: stopped=%d sig=%d status=%p\n",
			WIFSTOPPED(status), WSTOPSIG(status), (void*)status);
		stackTraceShadow(std::cerr, guest_ptr(0),  guest_ptr(0));
		assert (0 == 1 && "bad wait from breakpoint");
	}

	/* ptrace executes trap, so child process's IP needs to be fixed */
	return undoBreakpoint(child_pid);
}

bool PTImgChk::breakpointSysCalls(
	const guest_ptr ip_begin,
	const guest_ptr ip_end)
{
	return pt_arch->breakpointSysCalls(ip_begin, ip_end);
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

void PTImgChk::stackTraceShadow(
	std::ostream& os, guest_ptr begin, guest_ptr end)
{
	stackTrace(os, getBinaryPath(), child_pid, begin, end);
}

void PTImgChk::printMemory(std::ostream& os) const
{
	char	data[MEMLOG_BUF_SZ];
	void	*databuf[2] = {0, 0};

	if (!mem_log) return;

	if(!isMatchMemLog())
		os << "***";
	os << "last write @ " << (void*)mem_log->getAddress().o
		<< " size: " << mem_log->getSize() << ". ";

	readMemLogData(data);

	memcpy(databuf, data, mem_log->getSize());
	os << "ptrace_value: " << databuf[1] << "|" << databuf[0];

	mem->memcpy(&databuf[0], mem_log->getAddress(), mem_log->getSize());
	os << " vex_value: " << databuf[1] << "|" << databuf[0];

	memcpy(&databuf[0], mem_log->getData(), mem_log->getSize());
	os << " log_value: " << databuf[1] << "|" << databuf[0];

	os << std::endl;
}

void PTImgChk::printShadow(std::ostream& os) const
{
	pt_arch->printUserRegs(os);
	pt_arch->printFPRegs(os);
	printMemory(os);
}

void PTImgChk::printTraceStats(std::ostream& os)
{
	os	<< "Traced "
		<< blocks << " blocks, stepped "
		<< steps << " instructions" << std::endl;
}

void PTImgChk::stepSysCall(SyscallsMarshalled* sc_m)
{ pt_arch->stepSysCall(sc_m); }
