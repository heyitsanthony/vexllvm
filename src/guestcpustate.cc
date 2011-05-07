#include <stdio.h>

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
#define RSP_OFFSET	32
#define FS_SEG_OFFSET	(24*8)

GuestCPUState::GuestCPUState()
{
	mkRegCtx();
	state_data = new uint8_t[state_byte_c];
	memset(state_data, 0, state_byte_c);
	tls_data = new uint8_t[TLS_DATA_SIZE];
	memset(tls_data, 0, TLS_DATA_SIZE);
	*((uintptr_t*)(((uintptr_t)state_data) + FS_SEG_OFFSET)) = 
		(uintptr_t)tls_data;
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
	const Type		*i8ty, *i32ty, *i64ty, *i128ty;
	LLVMContext		&gctx(getGlobalContext());
	int			i;
	unsigned int		cur_byte_off, total_elems;

	i8ty = Type::getInt8Ty(gctx);
	i32ty = Type::getInt32Ty(gctx);
	i64ty = Type::getInt64Ty(gctx);
	i128ty = IntegerType::get(gctx, 128);

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
	{64, 16, "GPR"},	/* 0-15*/
	{64, 1, "CC_OP"},	/* 16 */
	{64, 1, "CC_DEP1"},	/* 17 */
	{64, 1, "CC_DEP2"},	/* 18 */
	{64, 1, "CC_NDEP"},	/* 19 */

	{64, 1, "DFLAG"},	/* 20 */
	{64, 1, "RIP"},		/* 21 */
	{64, 1, "ACFLAG"},	/* 22 */
	{64, 1, "IDFLAG"},	/* 23 */

	{64, 1, "FS_ZERO"},	/* 24 */

	{64, 1, "SSEROUND"},
	{128, 16, "XMM"},

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

/* gets the element number so we can do a GEP */
unsigned int GuestCPUState::byteOffset2ElemIdx(unsigned int off) const
{
	byte2elem_map::const_iterator it;
	it = off2ElemMap.find(off);
	assert (it != off2ElemMap.end() && "Could not resolve byte offset");
	return (*it).second;
}

void GuestCPUState::setStackPtr(void* stack_ptr)
{
	uint64_t*	rsp_ptr;
	rsp_ptr = ((uint64_t*)((uintptr_t)state_data + RSP_OFFSET));
	*rsp_ptr = (uint64_t)stack_ptr;
}
