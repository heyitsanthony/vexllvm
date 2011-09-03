#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Intrinsics.h>
#include <stdio.h>

#include "guestcpustate.h"
#include "cpu/amd64cpustate.h"
#include "cpu/armcpustate.h"
#include "cpu/i386cpustate.h"

using namespace llvm;

GuestCPUState::GuestCPUState()
: guestCtxTy(NULL)
, state_data(NULL)
, exit_type(NULL)
, state_byte_c(0)
{
}

GuestCPUState* GuestCPUState::create(Arch::Arch arch)
{
	switch(arch) {
	case Arch::X86_64:	return new AMD64CPUState();
	case Arch::I386:	return new I386CPUState();
	case Arch::ARM:		return new ARMCPUState();
	default:
		assert(!"supported guest architecture");
	}
}

uint8_t* GuestCPUState::copyStateData(void) const
{
	uint8_t	*ret;

	assert (state_data != NULL && "NO STATE DATA TO COPY??");
	ret = new uint8_t[getStateSize()];
	memcpy(ret, state_data, getStateSize());
	return ret;
}

Type* GuestCPUState::mkFromFields(
	struct guest_ctx_field* f,
	byte2elem_map& offmap)
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
		case 16:	t = i16ty;	break;
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

void GuestCPUState::print(std::ostream& os) const
{
	print(os, getStateData());
}

bool GuestCPUState::load(const char* fname)
{
	unsigned int	len = getStateSize();
	uint8_t		*data = (uint8_t*)getStateData();
	size_t		br;
	FILE		*f;

	f = fopen(fname, "rb");
	assert (f != NULL && "Could not load register file");
	if (f == NULL) return false;

	br = fread(data, len, 1, f);
	fclose(f);

	assert (br == 1 && "Error reading all register bytes");
	if (br != 1) return false;

	return true;
}

bool GuestCPUState::save(const char* fname)
{
	unsigned int	len = getStateSize();
	const uint8_t	*data = (const uint8_t*)getStateData();
	size_t		br;
	FILE		*f;

	f = fopen(fname, "w");
	if (!f) return false;

	assert (f != NULL);
	br = fwrite(data, len, 1, f);
	assert (br == 1);
	fclose(f);
	if (br != 1) return false;

	return true;
}