#include <unistd.h>
#include <llvm/IR/Intrinsics.h>
#include "genllvm.h"
#include "vexop.h"
#include "vexop_macros.h"
#include "vexhelpers.h"

using namespace llvm;

void vexop_setup_fp(VexHelpers* vh)
{
	const char*	lib_fname;

	if ((lib_fname = getenv("VEXLLVM_FPU")) == NULL)
		lib_fname = "softfloat.bc";

	vh->loadUserMod(lib_fname);
}

/* bit casts are important or the codegen will fuck up */
#define OPF0X_EMIT_SOFTFLOAT_OP(x,y,z)					\
	Function	*f = theVexHelpers->getHelper(z);		\
	assert (f != NULL && "Could not find "z);			\
	if (!lo_op_lhs->getType()->isIntegerTy())			\
		lo_op_lhs = builder->CreateBitCast(lo_op_lhs,		\
			get_i(lo_op_lhs->getType()->getPrimitiveSizeInBits())); \
	if (!lo_op_rhs->getType()->isIntegerTy())			\
		lo_op_rhs = builder->CreateBitCast(lo_op_rhs,		\
			get_i(lo_op_rhs->getType()->getPrimitiveSizeInBits())); \
	result = builder->CreateCall2(f, lo_op_lhs, lo_op_rhs);	

#define OPF0X_EMIT_CMP_SOFTFLOAT_OP(x,y,z)				\
	OPF0X_EMIT_SOFTFLOAT_OP(x,y,z);					\
	result = builder->CreateICmpNE(result,				\
		get_c(result->getType()->getPrimitiveSizeInBits(), 0));	\
	result = builder->CreateSExt(result, 				\
		get_i(y->getScalarType()->getPrimitiveSizeInBits()));	\
	result = builder->CreateBitCast(result, y->getScalarType());	\



#define OPF0X_SOFTFLOAT_EMIT(x,y,z)	\
	OPF0X_EMIT_BEGIN(x,y,z)		\
	OPF0X_EMIT_SOFTFLOAT_OP(x,y,z)	\
	OPF0X_EMIT_END(x,y,z)

#include <stdio.h>
#define OPF0X_CMP_SOFTFLOAT_EMIT(x,y,z)		\
	OPF0X_EMIT_BEGIN(x,y,z)			\
	OPF0X_EMIT_CMP_SOFTFLOAT_OP(x,y,z)	\
	OPF0X_EMIT_END(x,y,z)


OPF0X_SOFTFLOAT_EMIT(Add32F0x4, get_vt(4, 32), "float32_add")
OPF0X_SOFTFLOAT_EMIT(Add64F0x2, get_vt(2, 64), "float64_add")
OPF0X_SOFTFLOAT_EMIT(Div32F0x4, get_vt(4, 32), "float32_div")
OPF0X_SOFTFLOAT_EMIT(Div64F0x2, get_vt(2, 64), "float64_div")
OPF0X_SOFTFLOAT_EMIT(Mul32F0x4, get_vt(4, 32), "float32_mul")
OPF0X_SOFTFLOAT_EMIT(Mul64F0x2, get_vt(2, 64), "float64_mul")
OPF0X_SOFTFLOAT_EMIT(Sub32F0x4, get_vt(4, 32), "float32_sub")
OPF0X_SOFTFLOAT_EMIT(Sub64F0x2, get_vt(2, 64), "float64_sub")

BINOP_UNIMPL(CmpEQ32F0x4)
BINOP_UNIMPL(CmpEQ64F0x2)

OPF0X_CMP_SOFTFLOAT_EMIT(CmpLT32F0x4, get_vtf(4), "float32_lt");
OPF0X_CMP_SOFTFLOAT_EMIT(CmpLE32F0x4, get_vtf(4), "float32_le");
OPF0X_CMP_SOFTFLOAT_EMIT(CmpLT64F0x2, get_vtd(2), "float64_lt");
OPF0X_CMP_SOFTFLOAT_EMIT(CmpLE64F0x2, get_vtd(2), "float64_le");


BINOP_UNIMPL(CmpUN32F0x4)
BINOP_UNIMPL(CmpUN64F0x2)
BINOP_UNIMPL(Max32F0x4)
BINOP_UNIMPL(Max64F0x2)
BINOP_UNIMPL(Min32F0x4)
BINOP_UNIMPL(Min64F0x2)
UNOP_UNIMPL(RSqrtEst32F0x4)
//UNOP_UNIMPL(RSqrtEst64F0x2)
UNOP_UNIMPL(RecipEst32F0x4)
//UNOP_UNIMPL(Recip64F0x2)
UNOP_UNIMPL(Sqrt32F0x4)
UNOP_UNIMPL(Sqrt64F0x2)

BINOP_UNIMPL(Add32Fx2)
BINOP_UNIMPL(Add32Fx4)
BINOP_UNIMPL(Add64Fx2)
BINOP_UNIMPL(CmpEQ32Fx2)
BINOP_UNIMPL(CmpEQ32Fx4)
BINOP_UNIMPL(CmpEQ64Fx2)


Value * VexExprBinopCmpF64::emit(void) const
{
	llvm::Function	*f;
	BINOP_SETUP
	f = theVexHelpers->getHelper("vexop_softfloat_cmpf64");
	assert (f != NULL && "Could not find vexop_softfloat_cmpf64");
	v1 = builder->CreateBitCast(v1, get_i(64));
	v2 = builder->CreateBitCast(v2, get_i(64));
	return builder->CreateCall2(f, v1, v2);
}

BINOP_UNIMPL(CmpGE32Fx2)
BINOP_UNIMPL(CmpGE32Fx4)
BINOP_UNIMPL(CmpGT32Fx2)
BINOP_UNIMPL(CmpGT32Fx4)
BINOP_UNIMPL(CmpLE32Fx4)
BINOP_UNIMPL(CmpLE64Fx2)
BINOP_UNIMPL(CmpLT32Fx4)
BINOP_UNIMPL(CmpLT64Fx2)
BINOP_UNIMPL(CmpUN32Fx4)
BINOP_UNIMPL(CmpUN64Fx2)
BINOP_UNIMPL(Div32Fx4)
BINOP_UNIMPL(Div64Fx2)

#define EMIT_CONVERT_IGNORE_ROUNDING(x,y,z)	\
Value* VexExprBinop##x::emit(void) const	\
{						\
	llvm::Function	*f;			\
	BINOP_SETUP				\
	f = theVexHelpers->getHelper(y);	\
	assert (f != NULL);			\
	v2 = builder->CreateBitCast(v2, z);	\
	return builder->CreateCall(f, v2);	\
}


EMIT_CONVERT_IGNORE_ROUNDING(F64toF32, "float64_to_float32" ,get_i(64))
EMIT_CONVERT_IGNORE_ROUNDING(F64toI32S, "float64_to_int32", get_i(64))
EMIT_CONVERT_IGNORE_ROUNDING(F64toI32U, "float64_to_int32", get_i(64))
EMIT_CONVERT_IGNORE_ROUNDING(F64toI64S, "float64_to_int64", get_i(64))
EMIT_CONVERT_IGNORE_ROUNDING(I64StoF64, "int64_to_float64", get_i(64))
EMIT_CONVERT_IGNORE_ROUNDING(I64UtoF64, "int64_to_float64", get_i(64))

BINOP_UNIMPL(Max32Fx2)
BINOP_UNIMPL(Max32Fx4)
BINOP_UNIMPL(Max64Fx2)
BINOP_UNIMPL(Min32Fx2)
BINOP_UNIMPL(Min32Fx4)
BINOP_UNIMPL(Min64Fx2)
BINOP_UNIMPL(Mul32Fx2)
BINOP_UNIMPL(Mul32Fx4)
BINOP_UNIMPL(Mul64Fx2)
BINOP_UNIMPL(Sub32Fx2)
BINOP_UNIMPL(Sub32Fx4)
BINOP_UNIMPL(Sub64Fx2)

BINOP_UNIMPL(RoundF32toInt)
BINOP_UNIMPL(RoundF64toInt)

/* FIXME: ignores first parameter, rounding mode */
#define EMIT_HELPER_IGNORE_ROUNDING(x,y,z)		\
Value* VexExprTriop##x::emit(void) const		\
{							\
	llvm::Function	*f;				\
	TRIOP_SETUP					\
	f = theVexHelpers->getHelper(y);		\
	assert (f != NULL);				\
	v2 = builder->CreateBitCast(v2, z);		\
	v3 = builder->CreateBitCast(v3, z);		\
	return builder->CreateCall2(f, v2, v3);		\
}

EMIT_HELPER_IGNORE_ROUNDING(AddF32, "float32_add", get_i(32))
EMIT_HELPER_IGNORE_ROUNDING(AddF64, "float64_add", get_i(64))
EMIT_HELPER_IGNORE_ROUNDING(DivF32, "float32_div", get_i(32))
EMIT_HELPER_IGNORE_ROUNDING(DivF64, "float64_div", get_i(64))
EMIT_HELPER_IGNORE_ROUNDING(MulF32, "float32_mul", get_i(32))
EMIT_HELPER_IGNORE_ROUNDING(MulF64, "float64_mul", get_i(64))
EMIT_HELPER_IGNORE_ROUNDING(SubF32, "float32_sub", get_i(32))
EMIT_HELPER_IGNORE_ROUNDING(SubF64, "float64_sub", get_i(64))


#define EMIT_HELPER_SOFTFP_UNOP(x,y,z)			\
Value* VexExprUnop##x::emit(void) const			\
{							\
	llvm::Function	*f;			\
	UNOP_SETUP				\
	v1 = builder->CreateBitCast(v1, z);	\
	f = theVexHelpers->getHelper(y);	\
	assert (f != NULL);			\
	return builder->CreateCall(f, v1);	\
}

TRIOP_UNIMPL(PRemC3210F64)
TRIOP_UNIMPL(PRemF64)

EMIT_HELPER_SOFTFP_UNOP(F32toF64, "float32_to_float64", get_i(32))
EMIT_HELPER_SOFTFP_UNOP(I32StoF64, "int32_to_float64", get_i(32))
EMIT_HELPER_SOFTFP_UNOP(I32UtoF64, "int32_to_float64", get_i(32))
UNOP_UNIMPL(NegF32)
UNOP_UNIMPL(NegF64)
UNOP_UNIMPL(RSqrtEst32Fx4)
//UNOP_UNIMPL(RSqrt64Fx2)
UNOP_UNIMPL(RecipEst32Fx4)
//UNOP_UNIMPL(Recip64Fx2)
UNOP_UNIMPL(Sqrt32Fx4)
UNOP_UNIMPL(Sqrt64Fx2)
QOP_UNIMPL(MAddF64)
