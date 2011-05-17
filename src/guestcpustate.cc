#include <stdio.h>
#include "Sugar.h"

#include <iostream>
#include <llvm/Intrinsics.h>
#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <cstring>
#include <vector>

#include "guesttls.h"
#include "guestcpustate.h"
extern "C" {
#include <valgrind/libvex_guest_amd64.h>
}

using namespace llvm;

struct guest_ctx_field
{
	unsigned char	f_len;
	unsigned int	f_count;
	const char*	f_name;
};

#define state2amd64()	((VexGuestAMD64State*)(state_data))

#define FS_SEG_OFFSET	(24*8)

GuestCPUState::GuestCPUState()
{
	mkRegCtx();
	
	state_data = new uint8_t[state_byte_c+1];
	memset(state_data, 0, state_byte_c+1);
	exit_type = &state_data[state_byte_c];
	state2amd64()->guest_DFLAG = 1;

	tls = new GuestTLS();
	*((uintptr_t*)(((uintptr_t)state_data) + FS_SEG_OFFSET)) = 
		(uintptr_t)tls->getBase();
}

GuestCPUState::~GuestCPUState()
{
	delete [] state_data;
	delete tls;
}

Type* GuestCPUState::mkFromFields(
	struct guest_ctx_field* f, byte2elem_map& offmap)
{
	std::vector<const Type*>	types;
	const Type		*i8ty, *i16ty, *i32ty, *i64ty, *i128ty;
	LLVMContext		&gctx(getGlobalContext());
	int			i;
	unsigned int		cur_byte_off, total_elems;

	i8ty = Type::getInt8Ty(gctx);
	i16ty = Type::getInt16Ty(gctx);
	i32ty = Type::getInt32Ty(gctx);
	i64ty = Type::getInt64Ty(gctx);
	i128ty = VectorType::get(i16ty, 8);

	/* add all fields to types vector from structure */
	i = 0;
	cur_byte_off = 0;
	total_elems = 0;
	offmap.clear();
	while (f[i].f_len != 0) {
		const Type	*t;

		switch (f[i].f_len) {
		case 8:		t = i8ty;	break;
		case 32:	t = i32ty;	break;
		case 64:	t = i64ty;	break;
		case 128:	t = i128ty;	break;
		default:
		fprintf(stderr, "UGH w=%d\n", f[i].f_len);
		assert( 0 == 1 && "BAD FIELD WIDTH");
		}

		for (unsigned int c = 0; c < f[i].f_count; c++) {
			types.push_back(t);
			offmap[cur_byte_off] = total_elems;
			cur_byte_off += f[i].f_len / 8;
			total_elems++;
		}

		i++;
	}
	state_byte_c = cur_byte_off;

	return StructType::get(gctx, types, "guestCtxTy");
}

/* ripped from libvex_guest_amd64 */
static struct guest_ctx_field amd64_fields[] = 
{
	{64, 16, "GPR"},	/* 0-15, 8*16 = 128*/
	{64, 1, "CC_OP"},	/* 16, 128 */
	{64, 1, "CC_DEP1"},	/* 17, 136 */
	{64, 1, "CC_DEP2"},	/* 18, 144 */
	{64, 1, "CC_NDEP"},	/* 19, 152 */

	{64, 1, "DFLAG"},	/* 20, 160 */
	{64, 1, "RIP"},		/* 21, 168 */
	{64, 1, "ACFLAG"},	/* 22, 176 */
	{64, 1, "IDFLAG"},	/* 23 */

	{64, 1, "FS_ZERO"},	/* 24 */

	{64, 1, "SSEROUND"},
	{128, 17, "XMM"},	/* there is an XMM16 for valgrind stuff */

	{32, 1, "FTOP"},
	{64, 8, "FPREG"},
	{8, 8, "FPTAG"},
	
	{64, 1, "FPROUND"},
	{64, 1, "FC3210"},

	{32, 1, "EMWARN"},

	{64, 1, "TISTART"},
	{64, 1, "TILEN"},

	/* unredirected guest addr at start of translation whose
	 * start has been redirected */
	{64, 1, "NRADDR"},

	/* darwin hax */
	{64, 1, "SC_CLASS"},
	{64, 1, "GS_0x60"},
	{64, 1, "IP_AT_SYSCALL"},

	{64, 1, "pad"},
	/* END VEX STRUCTURE */

	{0}	/* time to stop */
};

void GuestCPUState::mkRegCtx(void) 
{
	/* XXX only support AMD64 right now. Update for other archs */
	guestCtxTy = mkFromFields(amd64_fields, off2ElemMap);
}

extern void dumpIRSBs(void);

/* gets the element number so we can do a GEP */
unsigned int GuestCPUState::byteOffset2ElemIdx(unsigned int off) const
{
	byte2elem_map::const_iterator it;
	it = off2ElemMap.find(off);
	if (it == off2ElemMap.end()) {
		unsigned int	c = 0;
		fprintf(stderr, "WTF IS AT %d\n", off);
		dumpIRSBs();
		for (int i = 0; amd64_fields[i].f_len; i++) {
			fprintf(stderr, "%s@%d\n", amd64_fields[i].f_name, c);
			c += (amd64_fields[i].f_len/8)*
				amd64_fields[i].f_count;
		}
		assert (0 == 1 && "Could not resolve byte offset");
	}
	return (*it).second;
}

void GuestCPUState::setStackPtr(void* stack_ptr)
{
	state2amd64()->guest_RSP = (uint64_t)stack_ptr;
}

void* GuestCPUState::getStackPtr(void) const
{
	return (void*)(state2amd64()->guest_RSP);
}

/**
 * %rax = syscall number
 * rdi, rsi, rdx, r10, r8, r9
 */
SyscallParams GuestCPUState::getSyscallParams(void) const
{
	return SyscallParams(
		state2amd64()->guest_RAX,
		state2amd64()->guest_RDI,
		state2amd64()->guest_RSI,
		state2amd64()->guest_RDX,
		state2amd64()->guest_R10,
		state2amd64()->guest_R8,
		state2amd64()->guest_R9);
}


void GuestCPUState::setSyscallResult(uint64_t ret)
{
	state2amd64()->guest_RAX = ret;
}

uint64_t GuestCPUState::getExitCode(void) const
{
	/* exit code is from call to exit(), which passes the exit
	 * code through the first argument */
	return state2amd64()->guest_RDI;
}

// 208 == XMM base
#define get_xmm_lo(i)	((uint64_t*)(&(((uint8_t*)state_data)[208+16*i])))[0]
#define get_xmm_hi(i)	((uint64_t*)(&(((uint8_t*)state_data)[208+16*i])))[1]

void GuestCPUState::print(std::ostream& os) const
{
	os << "RIP: " << (void*)state2amd64()->guest_RIP << "\n";
	os << "RAX: " << (void*)state2amd64()->guest_RAX << "\n";
	os << "RBX: " << (void*)state2amd64()->guest_RBX << "\n";
	os << "RCX: " << (void*)state2amd64()->guest_RCX << "\n";
	os << "RDX: " << (void*)state2amd64()->guest_RDX << "\n";
	os << "RSP: " << (void*)state2amd64()->guest_RSP << "\n";
	os << "RBP: " << (void*)state2amd64()->guest_RBP << "\n";
	os << "RDI: " << (void*)state2amd64()->guest_RDI << "\n";
	os << "RSI: " << (void*)state2amd64()->guest_RSI << "\n";
	os << "R8: " << (void*)state2amd64()->guest_R8 << "\n";
	os << "R9: " << (void*)state2amd64()->guest_R9 << "\n";
	os << "R10: " << (void*)state2amd64()->guest_R10 << "\n";
	os << "R11: " << (void*)state2amd64()->guest_R11 << "\n";
	os << "R12: " << (void*)state2amd64()->guest_R12 << "\n";
	os << "R13: " << (void*)state2amd64()->guest_R13 << "\n";
	os << "R14: " << (void*)state2amd64()->guest_R14 << "\n";
	os << "R15: " << (void*)state2amd64()->guest_R15 << "\n";

	for (int i = 0; i < 16; i++) {
		os
		<< "XMM" << i << ": "
		<< (void*) get_xmm_hi(i) << "|"
		<< (void*)get_xmm_lo(i) << std::endl;
	}

	const uint64_t*	tls_data = (const uint64_t*)tls->getBase();
	unsigned int	tls_bytes = tls->getSize();

	os << "&fs = " << (void*)(*((uint64_t*)(&state_data[192]))) << std::endl;
	for (unsigned int i = 0; i < tls_bytes / sizeof(uint64_t); i++) {
		if (!tls_data[i]) continue;
		os	<< "fs+" << (void*)(i*8)  << ":" 
			<< (void*)tls_data[i]
			<< std::endl;
	}
}

/* set a function argument */
void GuestCPUState::setFuncArg(uintptr_t arg_val, unsigned int arg_num)
{
	const int arg2reg[] = {
		offsetof(VexGuestAMD64State, guest_RDI),
		offsetof(VexGuestAMD64State, guest_RSI),
		offsetof(VexGuestAMD64State, guest_RDX),
		offsetof(VexGuestAMD64State, guest_RCX)
		};

	assert (arg_num <= 3);
	*((uint64_t*)((uintptr_t)state_data + arg2reg[arg_num])) = arg_val;
}
