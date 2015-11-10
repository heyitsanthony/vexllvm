#include <sys/ptrace.h>
#include <asm/ptrace-abi.h>
#include <sys/mman.h>

#include "cpu/i386_macros.h"
#include "cpu/i386cpustate.h"
#include "cpu/ptshadowi386.h"
#include "cpu/pti386cpustate.h"

#define WARNSTR "[PTimgI386] Warning: "
#define INFOSTR "[PTimgI386] Info: "

extern "C" {
#include "valgrind/libvex_guest_x86.h"
extern void x86g_dirtyhelper_CPUID_sse2 ( VexGuestX86State* st );
}

#define I386CPU	((I386CPUState*)gs->getCPUState())
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

PTShadowI386::PTShadowI386(GuestPTImg* gs, int in_pid)
: PTImgI386(gs, in_pid)
{}

bool PTShadowI386::isMatch(void) const { assert (0 == 1 && "STUB"); }

const VexGuestX86State& PTShadowI386::getVexState(void) const
{ return *((const VexGuestX86State*)gs->getCPUState()->getStateData()); }
VexGuestX86State& PTShadowI386::getVexState(void)
{ return *((VexGuestX86State*)gs->getCPUState()->getStateData()); }

bool PTShadowI386::isRegMismatch() const { assert (0 == 1 && "STUB"); }
void PTShadowI386::printFPRegs(std::ostream&) const { assert (0 == 1 && "STUB"); }
void PTShadowI386::printUserRegs(std::ostream&) const { assert (0 == 1 && "STUB"); }

void PTShadowI386::slurpRegisters(void)
{
	auto vgs = (VexGuestX86State*)gs->getCPUState()->getStateData();
	auto &regs = _pt_cpu->getRegs();

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
}

/* I might as well be writing an OS! */
void PTShadowI386::setupGDT(void)
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

bool PTShadowI386::readThreadEntry(unsigned idx, VEXSEG* buf)
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

bool PTShadowI386::patchCPUID(void)
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

void PTShadowI386::trapCPUIDs(uint64_t bias)
{
	foreach (it, patch_offsets.begin(), patch_offsets.end()) {
		uint64_t	addr = *it + bias;
		cpuid_insts[addr] = _pt_cpu->setBreakpoint(guest_ptr(addr));
	}
}

