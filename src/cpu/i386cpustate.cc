#include <stdio.h>
#include "Sugar.h"
#include <stddef.h>
#include <iostream>
#include <cstring>
#include <vector>

#include "guest.h"
#include "i386cpustate.h"
#include "sc_xlate.h"
extern "C" {
#include <valgrind/libvex_guest_x86.h>
}

#define state2i386()	((VexGuestX86State*)(state_data))

#define reg_field_ent_w_n(x, w, n) { #x, w, n, offsetof(VexGuestX86State, guest_##x), true }
#define reg_field_ent(x) reg_field_ent_w_n(x, sizeof(((VexGuestX86State*)0)->guest_##x), 1)
#define raw_field_ent_w_n(x, w, n) { #x, w, n, offsetof(VexGuestX86State, x), true }
#define raw_field_ent(x) raw_field_ent_w_n(x, sizeof((VexGuestX86State*)0)->x, 1)


/* ripped from libvex_guest_86 */
static struct guest_ctx_field x86_fields[] =
{
	raw_field_ent(host_EvC_FAILADDR),
	raw_field_ent(host_EvC_COUNTER),

	reg_field_ent(EAX),
	reg_field_ent(ECX),
	reg_field_ent(EDX),
	reg_field_ent(EBX),
	reg_field_ent(ESP),
	reg_field_ent(EBP),
	reg_field_ent(ESI),
	reg_field_ent(EDI),

	reg_field_ent(CC_OP),
	reg_field_ent(CC_DEP1),
	reg_field_ent(CC_DEP2),
	reg_field_ent(CC_NDEP),

	reg_field_ent(DFLAG),
	reg_field_ent(IDFLAG),
	reg_field_ent(ACFLAG),

	reg_field_ent(EIP),

	reg_field_ent_w_n(FPREG, 8, 8),
	reg_field_ent_w_n(FPTAG, 1, 8),
	reg_field_ent(FPROUND),
	reg_field_ent(FC3210),
	reg_field_ent(FTOP),

	reg_field_ent(SSEROUND),
	{ "XMM", 16, 8, offsetof(VexGuestX86State, guest_XMM0), true },


	reg_field_ent(CS),
	reg_field_ent(DS),
	reg_field_ent(ES),
	reg_field_ent(FS),
	reg_field_ent(GS),
	reg_field_ent(SS),

	reg_field_ent(LDT),
	reg_field_ent(GDT),

	reg_field_ent(EMNOTE),

	reg_field_ent(CMSTART),
	reg_field_ent(CMLEN),

	/* unredirected guest addr at start of translation whose
	 * start has been redirected */
	reg_field_ent(NRADDR),

	/* darwin hax */
	reg_field_ent(SC_CLASS),
	reg_field_ent(IP_AT_SYSCALL),
	raw_field_ent(padding1),
	/* END VEX STRUCTURE */

	{0}	/* time to stop */
};

I386CPUState::I386CPUState()
	: VexCPUState(x86_fields)
{
	state_byte_c = sizeof(VexGuestX86State);
	state_data = new uint8_t[state_byte_c+1];
	memset(state_data, 0, state_byte_c+1);
	exit_type = &state_data[state_byte_c];
	state2i386()->guest_DFLAG = 1;
	xlate = std::make_unique<I386SyscallXlate>();
}

void I386CPUState::noteRegion(const char* name, guest_ptr g)
{
	if (strcmp(name, "regs.ldt") == 0)
		state2i386()->guest_LDT = g.o;
	else if (strcmp(name, "regs.gdt") == 0)
		state2i386()->guest_GDT = g.o;
	else
		VexCPUState::noteRegion(name, g);
}

I386CPUState::~I386CPUState() {	delete [] state_data; }

void I386CPUState::setPC(guest_ptr ip) { state2i386()->guest_EIP = ip; }

guest_ptr I386CPUState::getPC(void) const
{ return guest_ptr(state2i386()->guest_EIP); }

void I386CPUState::setStackPtr(guest_ptr stack_ptr)
{
	state2i386()->guest_ESP = (uint64_t)stack_ptr;
}

guest_ptr I386CPUState::getStackPtr(void) const
{
	return guest_ptr(state2i386()->guest_ESP);
}

// 160 == XMM base
#define XMM_BASE offsetof(VexGuestX86State, guest_XMM0)
#define get_xmm_lo(x,i)	((const uint64_t*)(	\
	&(((const uint8_t*)x)[XMM_BASE+16*i])))[0]
#define get_xmm_hi(x,i)	((const uint64_t*)(	\
	&(((const uint8_t*)x)[XMM_BASE+16*i])))[1]

void I386CPUState::print(std::ostream& os, const void* regctx) const
{
	const VexGuestX86State	*s;
	
	s = (const VexGuestX86State*)regctx;
#define PRINT_GPR(X) os << #X": " << (void*)(uintptr_t)s->guest_##X << "\n";
	PRINT_GPR(EIP)
	PRINT_GPR(EAX)
	PRINT_GPR(EBX)
	PRINT_GPR(ECX)
	PRINT_GPR(EDX)
	PRINT_GPR(ESP)
	PRINT_GPR(EBP)
	PRINT_GPR(EDI)
	PRINT_GPR(ESI)

	for (int i = 0; i < 8; i++) {
		os
		<< "XMM" << i << ": "
		<< (void*) get_xmm_hi(s, i) << "|"
		<< (void*)get_xmm_lo(s, i) << std::endl;
	}

	for (int i = 0; i < 8; i++) {
		int r  = (s->guest_FTOP + i) & 0x7;
		os
		<< "ST" << i << ": "
		<< (void*)s->guest_FPREG[r] << std::endl;
	}

	const char* seg_tab[] = {"CS", "DS", "ES", "FS", "GS", "SS"};
	for (int i = 0; i < 6; i++) {
		os << seg_tab[i] << ": "
			<< (void*)(uintptr_t)((short*)(&s->guest_CS))[i] << '\n';
	}
}

#ifdef __i386__
void I386CPUState::setRegs(
	const user_regs_struct& regs, 
	const user_fpregs_struct& fpregs)
{
	state2i386()->guest_EAX = regs.eax;
	state2i386()->guest_ECX = regs.ecx;
	state2i386()->guest_EDX = regs.edx;
	state2i386()->guest_EBX = regs.ebx;
	state2i386()->guest_ESP = regs.esp;
	state2i386()->guest_EBP = regs.ebp;
	state2i386()->guest_ESI = regs.esi;
	state2i386()->guest_EDI = regs.edi;
	state2i386()->guest_EIP = regs.eip;

	//TODO: some kind of eflags, checking but i don't yet understand this
	//mess of broken apart state.

	//TODO: segments? valgrind does something less frakked up here 
	//(than amd64) ... there are actual segments... and no fs zero

	memcpy(	&state2i386()->guest_XMM0,
		&fpregs.xmm_space[0],
			sizeof(fpregs.xmm_space));

	//TODO: this is surely wrong, the sizes don't even match...
	memcpy(	&state2i386()->guest_FPREG[0],
		&fpregs.st_space[0],
		sizeof(state2i386()->guest_FPREG));

	//TODO: floating point flags and extra fp state, sse  rounding
}
#endif

unsigned int I386CPUState::getStackRegOff(void) const
{ return offsetof(VexGuestX86State, guest_ESP); }