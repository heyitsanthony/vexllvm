#include <stdio.h>
#include "Sugar.h"

#include <iostream>
#include <llvm/Intrinsics.h>
#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <cstring>
#include <vector>

#include "guestcpustate.h"

using namespace llvm;

struct guest_ctx_field
{
	unsigned char	f_len;
	unsigned int	f_count;
	const char*	f_name;
};

#define TLS_DATA_SIZE	4096	/* XXX. Good enough? */
/* indexes into GPR array */
#define AMD64_GPR_RAX	0
#define AMD64_GPR_RCX	1
#define AMD64_GPR_RDX	2
#define AMD64_GPR_RBX	3
#define AMD64_GPR_RSP	4
#define AMD64_GPR_RBP	5
#define AMD64_GPR_RSI	6
#define AMD64_GPR_RDI	7
#define AMD64_GPR_R8	8
#define AMD64_GPR_R9	9
#define AMD64_GPR_R10	10
#define AMD64_GPR_R11	11
#define AMD64_GPR_R12	12
#define AMD64_GPR_R13	13
#define AMD64_GPR_R14	14
#define AMD64_GPR_R15	15

#define get_gpr(x, y)		((uint64_t*)x)[y]
#define set_gpr(x, y, z)	((uint64_t*)x)[y] = z

#define FS_SEG_OFFSET	(24*8)

GuestCPUState::GuestCPUState()
{
	mkRegCtx();
	
	state_data = new uint8_t[state_byte_c+1];
	memset(state_data, 0, state_byte_c+1);
	exit_type = &state_data[state_byte_c];

	tls_data = new uint8_t[TLS_DATA_SIZE];
	memset(tls_data, 0, TLS_DATA_SIZE);
	*((uintptr_t*)(((uintptr_t)state_data) + FS_SEG_OFFSET)) = 
		(uintptr_t)tls_data;

	/* XXX pointer guard. LD uses the same pointer guard as the host
	 * process (luckily). We really need our own LD to get around this
	 * for other hosts. Alternative: force env LD_POINTER_GUARD=0 */
	uint64_t	ptr_guard;
	__asm__("movq %%fs:0x30, %0" : "=r" (ptr_guard));
	*((uint64_t*)(&tls_data[0x30])) = ptr_guard;
}

GuestCPUState::~GuestCPUState()
{
	delete [] state_data;
	delete [] tls_data;
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
	set_gpr(state_data, AMD64_GPR_RSP, (uint64_t)stack_ptr);
}

/**
 * %rax = syscall number
 * rdi, rsi, rdx, r10, r8, r9
 */
SyscallParams GuestCPUState::getSyscallParams(void) const
{
	return SyscallParams(
		get_gpr(state_data, AMD64_GPR_RAX),
		get_gpr(state_data, AMD64_GPR_RDI),
		get_gpr(state_data, AMD64_GPR_RSI),
		get_gpr(state_data, AMD64_GPR_RDX)); /* XXX more params? */
}


void GuestCPUState::setSyscallResult(uint64_t ret)
{
	set_gpr(state_data, AMD64_GPR_RAX, ret);
}

uint64_t GuestCPUState::getExitCode(void) const
{
	return get_gpr(state_data, AMD64_GPR_RAX);
}

// 208 == XMM base
#define get_xmm_lo(i)	((uint64_t*)(&(((uint8_t*)state_data)[208+16*i])))[0]
#define get_xmm_hi(i)	((uint64_t*)(&(((uint8_t*)state_data)[208+16*i])))[1]

void GuestCPUState::print(std::ostream& os) const
{
	os << "RIP: " << (void*)get_gpr(state_data, 21) << "\n";
	os << "RAX: " << (void*)get_gpr(state_data, AMD64_GPR_RAX) << "\n";
	os << "RAX: " << (void*)get_gpr(state_data, AMD64_GPR_RAX) << "\n";
	os << "RBX: " << (void*)get_gpr(state_data, AMD64_GPR_RBX) << "\n";
	os << "RCX: " << (void*)get_gpr(state_data, AMD64_GPR_RCX) << "\n";
	os << "RDX: " << (void*)get_gpr(state_data, AMD64_GPR_RDX) << "\n";
	os << "RSP: " << (void*)get_gpr(state_data, AMD64_GPR_RSP) << "\n";
	os << "RBP: " << (void*)get_gpr(state_data, AMD64_GPR_RBP) << "\n";
	os << "RDI: " << (void*)get_gpr(state_data, AMD64_GPR_RDI) << "\n";
	os << "RSI: " << (void*)get_gpr(state_data, AMD64_GPR_RSI) << "\n";
	os << "R8: " << (void*)get_gpr(state_data, AMD64_GPR_R8) << "\n";
	os << "R9: " << (void*)get_gpr(state_data, AMD64_GPR_R9) << "\n";
	os << "R10: " << (void*)get_gpr(state_data, AMD64_GPR_R10) << "\n";
	os << "R11: " << (void*)get_gpr(state_data, AMD64_GPR_R11) << "\n";
	os << "R12: " << (void*)get_gpr(state_data, AMD64_GPR_R12) << "\n";
	os << "R13: " << (void*)get_gpr(state_data, AMD64_GPR_R13) << "\n";
	os << "R14: " << (void*)get_gpr(state_data, AMD64_GPR_R14) << "\n";
	os << "R15: " << (void*)get_gpr(state_data, AMD64_GPR_R15) << "\n";

	for (int i = 0; i < 16; i++) {
		os
		<< "XMM" << i << ": "
		<< (void*) get_xmm_hi(i) << "|"
		<< (void*)get_xmm_lo(i) << std::endl;
	}

	os << "&fs = " << (void*)(*((uint64_t*)(&state_data[192]))) << std::endl;
	for (int i = 0; i < 8; i++) {
		os	<< "fs+" << (void*)(i*8)  << ":" 
			<< (void*)((uint64_t*)tls_data)[i]
			<< std::endl;;
	}
}
