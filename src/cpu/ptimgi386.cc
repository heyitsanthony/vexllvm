#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/ptrace.h>
#include <asm/ptrace-abi.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "cpu/ptimgi386.h"
#include "cpu/i386cpustate.h"

extern "C" {
#include "valgrind/libvex_guest_x86.h"
}

/* HAHA TOO BAD I CAN'T REUSE A HEADER. */
struct x86_user_regs
{
  long int ebx;
  long int ecx;
  long int edx;
  long int esi;
  long int edi;
  long int ebp;
  long int eax;
  long int xds;
  long int xes;
  long int xfs;
  long int xgs;
  long int orig_eax;
  long int eip;
  long int xcs;
  long int eflags;
  long int esp;
  long int xss;
};

struct x86_user_fpxregs
{
  unsigned short int cwd;
  unsigned short int swd;
  unsigned short int twd;
  unsigned short int fop;
  long int fip;
  long int fcs;
  long int foo;
  long int fos;
  long int mxcsr;
  long int reserved;
  long int st_space[32];   /* 8*16 bytes for each FP-reg = 128 bytes */
  long int xmm_space[32];  /* 8*16 bytes for each XMM-reg = 128 bytes */
  long int padding[56];
};


PTImgI386::PTImgI386(GuestPTImg* gs, int in_pid)
: PTImgArch(gs, in_pid)
{
	memset(shadow_user_regs, 0, sizeof(shadow_user_regs));
}

PTImgI386::~PTImgI386() {}

bool PTImgI386::isMatch(void) const { assert (0 == 1 && "STUB"); }
bool PTImgI386::doStep(guest_ptr start, guest_ptr end, bool& hit_syscall)
{ assert (0 == 1 && "STUB"); }

uintptr_t PTImgI386::getSysCallResult() const { assert (0 == 1 && "STUB"); }

const VexGuestX86State& PTImgI386::getVexState(void) const
{ return *((const VexGuestX86State*)gs->getCPUState()->getStateData()); }

bool PTImgI386::isRegMismatch() const { assert (0 == 1 && "STUB"); }
void PTImgI386::printFPRegs(std::ostream&) const { assert (0 == 1 && "STUB"); }
void PTImgI386::printUserRegs(std::ostream&) const { assert (0 == 1 && "STUB"); }
guest_ptr PTImgI386::getStackPtr() const { assert (0 == 1 && "STUB"); }

#define I386CPU	((I386CPUState*)gs->getCPUState())

void PTImgI386::slurpRegisters(void)
{

	int				err;
//	struct x86_user_regs		regs;
//	struct x86_user_fpxregs		fpregs;
	struct user_regs_struct		regs;
	struct user_fpregs_struct	fpregs;
	VexGuestX86State		*vgs;

	err = ptrace((__ptrace_request)PTRACE_GETREGS, child_pid, NULL, &regs);
	assert(err != -1);

	err = ptrace(
		(__ptrace_request)PTRACE_GETFPREGS,
		child_pid, NULL, &fpregs);
	assert(err != -1);

	vgs = (VexGuestX86State*)gs->getCPUState()->getStateData();

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
		setupGDT();
	}

	fprintf(stderr, "[PTimgI386] Warning: eflags not computed\n");
	fprintf(stderr, "[PTimgI386] Warning: FP, TLS, etc not supported\n");
}


/* I might as well be writing an OS! */
void PTImgI386::setupGDT(void)
{
	guest_ptr	gdt, ldt;
	VEXSEG		default_ent;

	fprintf(stderr, "[PTimgI386] Warning: Bogus GDT. Bogus LDT\n");

	default_ent.LdtEnt.Bits.LimitLow = 0xffff;
	default_ent.LdtEnt.Bits.BaseLow = 0;
	default_ent.LdtEnt.Bits.BaseMid = 0;
	default_ent.LdtEnt.Bits.Type = 0; /* ??? */
	default_ent.LdtEnt.Bits.Dpl = 3;
	default_ent.LdtEnt.Bits.Pres = 1;
	default_ent.LdtEnt.Bits.LimitHi = 0xf;
	default_ent.LdtEnt.Bits.Sys = 0; /* avail for sys use */
	default_ent.LdtEnt.Bits.Reserved_0 = 0;
	default_ent.LdtEnt.Bits.Default_Big = 1;
	default_ent.LdtEnt.Bits.Granularity = 1; /* ??? */
	default_ent.LdtEnt.Bits.BaseHi = 0;

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

	fprintf(stderr, "TLS[%d]: base=%p. limit=%p\n"
		"seg_32=%d. contents=%d. seg_not_present=%d. useable=%d\n",
		idx, (void*)ud.base_addr, (void*)ud.limit,
		ud.seg_32bit, ud.contents, ud.seg_not_present, ud.useable);

	/* translate linux user desc into vex/hw ldt */
	buf->LdtEnt.Bits.LimitLow = ud.limit & 0xffff;
	buf->LdtEnt.Bits.BaseLow = ud.base_addr & 0xffff;
	buf->LdtEnt.Bits.BaseMid = (ud.base_addr >> 16) & 0xff;
	buf->LdtEnt.Bits.Type = 0; /* ??? */
	buf->LdtEnt.Bits.Dpl = 3;
	buf->LdtEnt.Bits.Pres = !ud.seg_not_present;
	buf->LdtEnt.Bits.LimitHi = ud.limit >> 16;
	buf->LdtEnt.Bits.Sys = 0;
	buf->LdtEnt.Bits.Reserved_0 = 0;
	buf->LdtEnt.Bits.Default_Big = 1;
	buf->LdtEnt.Bits.Granularity = ud.limit_in_pages;
	buf->LdtEnt.Bits.BaseHi = (ud.base_addr >> 24) & 0xff;

	return true;
}


void PTImgI386::stepSysCall(SyscallsMarshalled*) { assert (0 == 1 && "STUB"); }

long int PTImgI386::setBreakpoint(guest_ptr addr)
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

void PTImgI386::resetBreakpoint(guest_ptr addr, long v)
{
	int err = ptrace(PTRACE_POKETEXT, child_pid, addr.o, v);
	assert (err != -1 && "Failed to reset breakpoint");
}

guest_ptr PTImgI386::undoBreakpoint()
{
	struct x86_user_regs	regs;
	int			err;

	/* should be halted on our trapcode. need to set rip prior to
	 * trapcode addr */
	err = ptrace((__ptrace_request)PTRACE_GETREGS, child_pid, NULL, &regs);
	assert (err != -1);

	regs.eip--; /* backtrack before int3 opcode */
	err = ptrace((__ptrace_request)PTRACE_SETREGS, child_pid, NULL, &regs);

	/* run again w/out reseting BP and you'll end up back here.. */
	return guest_ptr(regs.eip);
}

guest_ptr PTImgI386::getPC() { assert (0 == 1 && "STUB"); }
bool PTImgI386::canFixup(
	const std::vector<std::pair<guest_ptr, unsigned char> >&,
	bool has_memlog) const { assert (0 == 1 && "STUB"); }
bool PTImgI386::breakpointSysCalls(guest_ptr, guest_ptr) { assert (0 == 1 && "STUB"); }
void PTImgI386::revokeRegs() { assert (0 == 1 && "STUB"); }
