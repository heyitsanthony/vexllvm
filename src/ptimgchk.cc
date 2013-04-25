#include <errno.h>
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

#include "Sugar.h"
#include "syscall/syscallsmarshalled.h"
#include "ptimgchk.h"
#include "memlog.h"
#include "guestptmem.h"
#include "guestcpustate.h"

/*
 * single step shadow program while counter is in specific range
 */
PTImgChk::PTImgChk(const char* binname, bool use_entry)
: GuestPTImg(binname, use_entry)
, bp_steps(0)
, blocks(0)
, hit_syscall(false)
, mem_log(getenv("VEXLLVM_LAST_STORE") ? new MemLog() : NULL)
, xchk_stack(getenv("VEXLLVM_XCHK_STACK") ? true : false)
, fixup_c(0)
{
	log_steps = (getenv("VEXLLVM_LOG_STEPS")) ? true : false;
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

	pt_arch->incBlocks();
	return pt_arch->getPC();
}

/* if we find an opcode that is causing problems, we'll replace the vexllvm
 * state with the pt state (since the pt state is by definition correct)
 */
bool PTImgChk::fixup(const std::vector<InstExtent>& insts)
{
	FixupDir	fixup;

	fixup = pt_arch->canFixup(insts, mem_log != NULL);
	if (fixup == FIXUP_NONE) {
		fprintf(stderr, "VAIN ATTEMPT TO FIXUP %p-%p\n",
			(void*)(insts.front().first.o),
			(void*)(insts.back().first.o));
		/* couldn't figure out how to fix */
		return false;
	}

	fixup_c++;
	if (fixup == FIXUP_NATIVE)
		doFixupNative();
	else
		doFixupGuest();

	if (mem_log) mem_log->clear();

	return true;
}

void PTImgChk::pushPage(guest_ptr p)
{
	GuestMem*		m(getMem());
	GuestMem::Mapping	mp;
	GuestPTMem		ptmem(const_cast<PTImgChk*>(this), child_pid);
	char			buf[4096];

	p.o &= ~0xfffUL;

	if (m->lookupMapping(guest_ptr(p), mp) == false)
		return;

	if (!(mp.getReqProt() & PROT_WRITE))
		return;

	m->memcpy(buf, p, 4096);
	ptmem.memcpy(p, buf, 4096);
}

void PTImgChk::doFixupNative(void)
{
	GuestCPUState*	gcpu;
	const uint64_t*	dat;
	unsigned	dat_elems;

	std::cerr << "FIXUP THE NATIVE STATE\n";

	pushRegisters();

	/* load all nearby pointers */

	/* XXX: this is really stupid, need a better interface
	 * for enumerating all registers */
	gcpu = getCPUState();
	dat = (const uint64_t*)gcpu->getStateData();
	dat_elems = gcpu->getStateSize() / sizeof(*dat);

	/* XXX: should copy in page prior too */
	for (unsigned i = 0; i < dat_elems; i++) {
		uint64_t		reg = dat[i];
		if (reg < 0x1000) continue;
		pushPage(guest_ptr(reg - 4096));
		pushPage(guest_ptr(reg));
	}
}

void PTImgChk::doFixupGuest(void)
{
	GuestCPUState*	gcpu;
	GuestMem*	m;
	GuestPTMem	ptmem(this, child_pid);
	const uint64_t*	dat;
	unsigned	dat_elems;

	slurpRegisters(child_pid);

	/* load all nearby pointers */

	/* XXX: this is really stupid, need a better interface
	 * for enumerating all registers */
	gcpu = getCPUState();
	dat = (const uint64_t*)gcpu->getStateData();
	dat_elems = gcpu->getStateSize() / sizeof(*dat);

	m = getMem();

	/* XXX: should copy in page prior too */
	for (unsigned i = 0; i < dat_elems; i++) {
		GuestMem::Mapping	mp;
		uint64_t		reg = dat[i];
		char			buf[4096];

		if (m->lookupMapping(guest_ptr(reg), mp) == false)
			continue;

		if (!(mp.getReqProt() & PROT_WRITE))
			continue;

		ptmem.memcpy(buf, guest_ptr(reg & ~(0xfffUL)), 4096);
		m->memcpy(guest_ptr(reg & ~(0xfffUL)), buf, 4096);
	}
}

bool PTImgChk::isMatch() const
{
	if (!pt_arch->isMatch())
		return false;

	if (!isStackMatch())
		return false;

	if (!isMatchMemLog())
		return false;

	return true;
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

uintptr_t PTImgChk::getSysCallResult(void) const
{ return pt_arch->getSysCallResult(); }

guest_ptr PTImgChk::stepToBreakpoint(void)
{
	int	err, status;

	bp_steps++;
	pt_arch->revokeRegs();
	err = ptrace(PTRACE_CONT, child_pid, NULL, NULL);
	if(err < 0) {
		perror("PTImgChk::doStep ptrace single step");
		exit(1);
	}
	wait(&status);

	if (!(WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP)) {
		fprintf(stderr,
			"OOPS. status: stopped=%d sig=%d status=%p\n",
			WIFSTOPPED(status), WSTOPSIG(status),
			(void*)(long)status);
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

guest_ptr PTImgChk::getPageMismatch(guest_ptr p) const
{
	GuestMem::Mapping	mp;
	GuestPTMem		ptmem(const_cast<PTImgChk*>(this), child_pid);
	char			buf_pt[4096], buf_vex[4096];

	p.o = p.o & ~0xfffUL;

	/* no match found, so impossible to have mismatch */
	if (getMem()->lookupMapping(p, mp) == false)
		return guest_ptr(0);

	/* can not write to this page, don't bother checking */
	if (!(mp.getReqProt() & PROT_WRITE))
		return guest_ptr(0);

	ptmem.memcpy(buf_pt, p, 4096);
	getMem()->memcpy(buf_vex, p, 4096);

	for (unsigned i = 0; i < 4096; i++)
		if (buf_pt[i] != buf_vex[i])
			return guest_ptr(p.o + i);

	return guest_ptr(0);
}

void PTImgChk::printRootTrace(std::ostream& os) const
{
	const GuestCPUState	*gcpu;
	const uint64_t		*dat;
	unsigned		dat_elems;

	gcpu = getCPUState();
	dat = (const uint64_t*)gcpu->getStateData();
	dat_elems = gcpu->getStateSize() / sizeof(*dat);

	for (unsigned i = 0; i < dat_elems; i++) {
		guest_ptr mismatch_ptr;

		mismatch_ptr = getPageMismatch(guest_ptr(dat[i] - 4096));
		if (mismatch_ptr) {
			os	<< "[PTImgChk] Mismatch on ptr="
				<< (void*)(mismatch_ptr.o) << '\n';
		}

		mismatch_ptr = getPageMismatch(guest_ptr(dat[i]));
		if (mismatch_ptr) {
			os	<< "[PTImgChk] Mismatch on ptr="
				<< (void*)(mismatch_ptr.o) << '\n';
		}
	}
}

void PTImgChk::printShadow(std::ostream& os) const
{
	pt_arch->printUserRegs(os);
	pt_arch->printFPRegs(os);

	printMemory(os);
	printRootTrace(os);
}

void PTImgChk::ignoreSysCall(void) { pt_arch->ignoreSysCall(); }

void PTImgChk::printTraceStats(std::ostream& os)
{
	os	<< "Traced "
		<< blocks << " blocks, stepped "
		<< pt_arch->getSteps() << " instructions" << std::endl;
}

void PTImgChk::stepSysCall(SyscallsMarshalled* sc_m)
{ pt_arch->stepSysCall(sc_m); }

void PTImgChk::pushRegisters(void) { pt_arch->pushRegisters(); }
