#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <asm/ptrace-abi.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "Sugar.h"
#include "cpu/i386_macros.h"
#include "cpu/ptimgi386.h"
#include "cpu/pti386cpustate.h"
#include "cpu/i386cpustate.h"

#define WARNSTR "[PTimgI386] Warning: "
#define INFOSTR "[PTimgI386] Info: "

extern "C" {
#include "valgrind/libvex_guest_x86.h"
extern void x86g_dirtyhelper_CPUID_sse2 ( VexGuestX86State* st );
}

#define _pt_cpu	((PTI386CPUState*)pt_cpu.get())

/* from linux sources. whatever */
struct user_desc {
	unsigned int  entry_number;
	unsigned int  base_addr;
	unsigned int  limit;
	unsigned int  seg_32bit:1;
	unsigned int  contents:2;
	unsigned int  read_exec_only:1;
	unsigned int  limit_in_pages:1;
	unsigned int  seg_not_present:1;
	unsigned int  useable:1;
	unsigned int  lm:1;
};

PTImgI386::PTImgI386(GuestPTImg* gs, int in_pid)
: PTImgArch(gs, in_pid)
{
	if (in_pid != 0)
		pt_cpu = std::make_unique<PTI386CPUState>(in_pid);
}

PTImgI386::~PTImgI386() {}

void PTImgI386::setPID(int in_pid)
{
	child_pid = in_pid;
	pt_cpu = std::make_unique<PTI386CPUState>(in_pid);
}

bool PTImgI386::isMatch(void) const { assert (0 == 1 && "STUB"); }
bool PTImgI386::doStep(guest_ptr start, guest_ptr end, bool& hit_syscall)
{ assert (0 == 1 && "STUB"); }

const VexGuestX86State& PTImgI386::getVexState(void) const
{ return *((const VexGuestX86State*)gs->getCPUState()->getStateData()); }

bool PTImgI386::isRegMismatch() const { assert (0 == 1 && "STUB"); }
void PTImgI386::printFPRegs(std::ostream&) const { assert (0 == 1 && "STUB"); }
void PTImgI386::printUserRegs(std::ostream&) const { assert (0 == 1 && "STUB"); }

#define I386CPU	((I386CPUState*)gs->getCPUState())

void PTImgI386::slurpRegisters(void)
{
	auto vgs = (VexGuestX86State*)gs->getCPUState()->getStateData();
	auto regs = _pt_cpu->getRegs();

	vgs->guest_EAX = regs.orig_rax;
	vgs->guest_EBX = regs.rbx;
      	vgs->guest_ECX = regs.rcx;
      	vgs->guest_EDX = regs.rdx;
      	vgs->guest_ESI = regs.rsi;
	vgs->guest_ESP = regs.rsp;
	vgs->guest_EBP = regs.rbp;
	vgs->guest_EDI = regs.rdi;
	vgs->guest_EIP = regs.rip;

	vgs->guest_CS = regs.cs;
	vgs->guest_DS = regs.ds;
	vgs->guest_ES = regs.es;
	vgs->guest_FS = regs.fs;
	vgs->guest_GS = regs.gs;
	vgs->guest_SS = regs.ss;

	/* allocate GDT */
	if (!vgs->guest_GDT && !vgs->guest_LDT) {
		fprintf(stderr, INFOSTR "gs=%p. fs=%p\n",
			(void*)regs.gs, (void*)regs.fs);
		setupGDT();
	}

	fprintf(stderr, WARNSTR "eflags not computed\n");
	fprintf(stderr, WARNSTR "FP, TLS, etc not supported\n");
}

/* I might as well be writing an OS! */
void PTImgI386::setupGDT(void)
{
	guest_ptr		gdt, ldt;
	VEXSEG			default_ent;
	struct user_desc	ud;

	fprintf(stderr, WARNSTR "Bogus GDT. Bogus LDT\n");

	memset(&ud, 0, sizeof(ud));
	ud.limit = ~0;
	ud.limit_in_pages = 1;
	ud2vexseg(ud, &default_ent.LdtEnt);

	if (gs->getMem()->mmap(
		gdt,
		guest_ptr(0),
		VEX_GUEST_X86_GDT_NENT*sizeof(VEXSEG),
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS,
		-1,
		0) == 0)
	{
		VEXSEG	*segs;
		I386CPU->setGDT(gdt);
		segs = (VEXSEG*)gs->getMem()->getHostPtr(gdt);
		for (unsigned i = 0; i < VEX_GUEST_X86_GDT_NENT; i++) {
			if (readThreadEntry(i, &segs[i]))
				continue;
			segs[i] = default_ent;
		}

		gs->getMem()->nameMapping(gdt, "gdt");
	}

	if (gs->getMem()->mmap(
		ldt,
		guest_ptr(0),
		VEX_GUEST_X86_LDT_NENT*sizeof(VEXSEG),
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS,
		-1,
		0) == 0)
	{
		VEXSEG	*segs;
		I386CPU->setLDT(ldt);
		segs = (VEXSEG*)gs->getMem()->getHostPtr(ldt);
		for (unsigned i = 0; i < VEX_GUEST_X86_LDT_NENT; i++) {
			if (readThreadEntry(i, &segs[i]))
				continue;
			segs[i] = default_ent;
		}

		gs->getMem()->nameMapping(ldt, "ldt");
	}
}

bool PTImgI386::readThreadEntry(unsigned idx, VEXSEG* buf)
{
	int			err;
	struct user_desc	ud;

	err = ptrace(
		(__ptrace_request)PTRACE_GET_THREAD_AREA,
		child_pid,
		idx,
		&ud);
	if (err == -1)
		return false;

	fprintf(stderr,
		INFOSTR "TLS[%d]: base=%p. limit=%p\n"
		INFOSTR "seg32=%d. contents=%d. not_present=%d. useable=%d\n",
		idx, (void*)(intptr_t)ud.base_addr, (void*)(intptr_t)ud.limit,
		ud.seg_32bit, ud.contents, ud.seg_not_present, ud.useable);

	/* translate linux user desc into vex/hw ldt */
	ud2vexseg(ud, &buf->LdtEnt);
	return true;
}

GuestPTImg::FixupDir PTImgI386::canFixup(
	const std::vector<std::pair<guest_ptr, unsigned char> >&,
	bool has_memlog) const { assert (0 == 1 && "STUB"); }

void PTImgI386::setFakeInfo(const char* info_file)
{
	FILE*		f;
	uint32_t	cur_off;

	f = fopen(info_file, "rb");
	assert (f != NULL && "could not open patch file");

	while (fscanf(f, "%x\n", &cur_off) > 0)
		patch_offsets.insert(cur_off);

	assert (patch_offsets.size() > 0 && "No patch offsets?");

	fprintf(stderr,
		INFOSTR "loaded %d CPUID patch offsets\n",
		(int)patch_offsets.size());

	fclose(f);
}

#define IS_PEEKTXT_CPUID(x)	(((x) & 0xffff) == 0xa20f)
#define IS_PEEKTXT_INT3(x)	(((x) & 0xff) == 0xcc)

uint64_t PTImgI386::checkCPUID(void)
{
	struct user_regs_struct regs;
	int			err;
	uint64_t		v;

	err = ptrace((__ptrace_request)PTRACE_GETREGS, child_pid, NULL, &regs);
	assert(err != -1);

	v = ptrace(PTRACE_PEEKTEXT, child_pid, regs.rip, NULL);

	if (IS_PEEKTXT_INT3(v))
		return 1;

	if (IS_PEEKTXT_CPUID(v))
		return regs.rip;

	return 0;
}

bool PTImgI386::patchCPUID(void)
{
	struct user_regs_struct regs;
	VexGuestX86State	fakeState;
	int			err;
	static bool		has_patched = false;

	err = ptrace((__ptrace_request)PTRACE_GETREGS, child_pid, NULL, &regs);
	assert(err != -1);

	/* -1 because the trap opcode is dispatched */
	if (has_patched && cpuid_insts.count(regs.rip-1) == 0) {
		fprintf(stderr, INFOSTR "unpatched addr %p\n", (void*)regs.rip);
		return false;
	}

	fakeState.guest_EAX = regs.rax;
	fakeState.guest_EBX = regs.rbx;
	fakeState.guest_ECX = regs.rcx;
	fakeState.guest_EDX = regs.rdx;
	x86g_dirtyhelper_CPUID_sse2(&fakeState);
	regs.rax = fakeState.guest_EAX;
	regs.rbx = fakeState.guest_EBX;
	regs.rcx = fakeState.guest_ECX;
	regs.rdx = fakeState.guest_EDX;

	fprintf(stderr, INFOSTR "patched CPUID @ %p\n", (void*)regs.rip);

	/* skip over the extra byte from the cpuid opcode */
	regs.rip += (has_patched) ? 1 : 2;

	err = ptrace((__ptrace_request)PTRACE_SETREGS, child_pid, NULL, &regs);
	assert (err != -1);

	has_patched = true;
	return true;
}

/* patch all cpuid instructions */
bool PTImgI386::stepInitFixup(void)
{
	int		err, status, prefix_ins_c = 0;
	uint64_t	cpuid_pc = 0, bias = 0;

	/* single step until cpuid instruction */
	while ((cpuid_pc = checkCPUID()) == 0) {
		int	ok;

		err = ptrace(PTRACE_SINGLESTEP, child_pid, NULL, NULL);
		assert (err != -1 && "Bad PTRACE_SINGLESTEP");
		wait(&status);

		ok = (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP);
		if (!ok) {
			if (	WIFSTOPPED(status) &&
				WSTOPSIG(status) == SIGSEGV)
			{
				fprintf(stderr,
					"sigsegv. prefix_ins_c=%d\n",
					prefix_ins_c);
			}

			fprintf(stderr,
				"[PTImgI386] mixed signals! status=%x\n",
				status);
			return false;
		}

		prefix_ins_c++;
	}

	/* int3 => terminate early-- can't trust more code */
	if (cpuid_pc == 1) {
		struct user_regs_struct regs;
		fprintf(stderr, INFOSTR "skipping CPUID patching\n");
		ptrace((__ptrace_request)PTRACE_GETREGS, child_pid, NULL, &regs);
		/* undoBreakpoint code expects the instruction to already
		 * be dispatched, so fake it by bumping the PC */
		regs.rip++; 
		ptrace((__ptrace_request)PTRACE_SETREGS, child_pid, NULL, &regs);
		return true;
	}

	/* find bias of cpuid instruction w/r/t patch offsets */
	foreach (it, patch_offsets.begin(), patch_offsets.end()) {
		/* XXX: "research quality" */
		if ((*it & 0xfff) == (cpuid_pc & 0xfff)) {
			bias = cpuid_pc - *it;
			break;
		}
	}

	fprintf(stderr, INFOSTR "CPUID patch bias %p\n", (void*)bias);

	assert (bias != 0 && "Expected non-zero bias. Bad patch file?");

	/* now have offset of first CPUID instruction.
	 * replace all CPUID instructions with trap instruction. */
	foreach (it, patch_offsets.begin(), patch_offsets.end()) {
		uint64_t	addr = *it + bias;
		cpuid_insts[addr] = _pt_cpu->setBreakpoint(guest_ptr(addr));
	}

	/* trap on replaced cpuid instructions until non-cpuid instruction */
	while (patchCPUID() == true) {
		err = ptrace(PTRACE_CONT, child_pid, NULL, NULL);
		assert (err != -1 && "bad ptrace_cont");
		wait(&status);
		assert (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP);
	}

	/* child process should be on a legitimate int3 instruction now */

	/* restore CPUID instructions */
	for (const auto& p : cpuid_insts) {
		_pt_cpu->resetBreakpoint(guest_ptr(p.first), p.second);
	}

	return true;
}
