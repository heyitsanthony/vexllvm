#include <llvm/IR/Intrinsics.h>
#include <stdio.h>
#include "genllvm.h"
#include "vexop.h"
#include "vexop_macros.h"
#include "vexhelpers.h"

using namespace llvm;

/* FP already handled with default helper modules */
void vexop_setup_fp(VexHelpers* vh) {}

#define VINT(y,z)							\
	Function	*f;						\
	std::vector<llvm::Type*> call_args;				\
	call_args.push_back(y);						\
	f = Intrinsic::getDeclaration(					\
		theGenLLVM->getModule(),				\
		Intrinsic::z, ArrayRef<Type*>(call_args)); 		\
	assert (f != NULL);						\
	v1 = builder->CreateCall(f, v1);				

#define VRCP(o) v1 = builder->CreateFDiv(o, v1);

#define DBL_1x2 get_cv(g_dbl_1x2)
#define FLT_1x4 get_cv(g_flt_1x4)

#define UNOP_NEG_EMIT(x, y, c)  \
Value* VexExprUnop##x::emit(void) const		\
{						\
	UNOP_SETUP				\
	v1 = builder->CreateBitCast(v1, y);	\
	return builder->CreateFSub(c, v1);	\
}

#define TRIOP_EXPAND_EMIT(x,y,z,w)				\
Value* VexExprTriop##x::emit(void) const			\
{								\
	TRIOP_SETUP						\
	v2 = builder->Create##y(v2, z);				\
	v3 = builder->Create##y(v3, z);				\
	return builder->Create##w(v2, v3);			\
}

#define OPV_RSQ(x, y, z, a, b)						\
Value* VexExprUnop##x::emit(void) const					\
{									\
	UNOP_SETUP							\
	v1 = builder->CreateBitCast(v1, y);				\
	a								\
	b								\
	return v1;							\
}


// Constant* const g_dbl_0x2[] = { DBL_0, DBL_0 };
Constant* const g_flt_0x2[] = { FLT_0, FLT_0 };
Constant* const g_flt_0x4[] = { FLT_0, FLT_0, FLT_0, FLT_0 };
Constant* const g_dbl_1x2[] = { DBL_1, DBL_1 };
Constant* const g_flt_1x4[] = { FLT_1, FLT_1, FLT_1, FLT_1 };

X_TO_Y_EMIT(F32toF64, CreateFPExt, get_f(),    get_d())
X_TO_Y_EMIT(I32StoF64, CreateSIToFP, get_i(32), get_d())
X_TO_Y_EMIT(I32UtoF64, CreateUIToFP, get_i(32), get_d())

X_TO_Y_EMIT_ROUND(F64toF32, CreateFPTrunc, get_d(), get_f())
X_TO_Y_EMIT_ROUND(I64StoF64, CreateSIToFP, get_i(64), get_d())
X_TO_Y_EMIT_ROUND(I64UtoF64, CreateUIToFP, get_i(64), get_d())
X_TO_Y_EMIT_ROUND(F64toI32S, CreateFPToSI, get_d(), get_i(32))
X_TO_Y_EMIT_ROUND(F64toI32U, CreateFPToSI, get_d(), get_i(32))
X_TO_Y_EMIT_ROUND(F64toI64S, CreateFPToSI, get_d(), get_i(64))

BINCMP_EMIT(Min32Fx2, get_vtf(2), FCmpULT);
BINCMP_EMIT(Max32Fx2, get_vtf(2), FCmpUGT);
BINCMP_EMIT(Min32Fx4, get_vtf(4), FCmpULT);
BINCMP_EMIT(Max32Fx4, get_vtf(4), FCmpUGT);

BINCMP_EMIT(Max64Fx2, get_vtd(2), FCmpUGT);
BINCMP_EMIT(Min64Fx2, get_vtd(2), FCmpULT);

OPF0X_EMIT(Mul64F0x2, get_vtd(2), FMul)
OPF0X_EMIT(Div64F0x2, get_vtd(2), FDiv)
OPF0X_EMIT(Add64F0x2, get_vtd(2), FAdd)
OPF0X_EMIT(Sub64F0x2, get_vtd(2), FSub)
OPF0X_EMIT(Mul32F0x4, get_vtf(4), FMul)
OPF0X_EMIT(Div32F0x4, get_vtf(4), FDiv)
OPF0X_EMIT(Add32F0x4, get_vtf(4), FAdd)
OPF0X_EMIT(Sub32F0x4, get_vtf(4), FSub)

OPF0X_CMP_EMIT(CmpLT32F0x4, get_vtf(4), FCmpOLT);
OPF0X_CMP_EMIT(CmpLE32F0x4, get_vtf(4), FCmpOLE);
OPF0X_CMP_EMIT(CmpEQ32F0x4, get_vtf(4), FCmpOEQ);
OPF0X_CMP_EMIT(CmpUN32F0x4, get_vtf(4), FCmpUNO);

OPF0X_CMP_EMIT(CmpLT64F0x2, get_vtd(2), FCmpOLT);
OPF0X_CMP_EMIT(CmpLE64F0x2, get_vtd(2), FCmpOLE);
OPF0X_CMP_EMIT(CmpEQ64F0x2, get_vtd(2), FCmpOEQ);
OPF0X_CMP_EMIT(CmpUN64F0x2, get_vtd(2), FCmpUNO);

OPF0X_SEL_EMIT(Max32F0x4, get_vtf(4), FCmpUGT)
OPF0X_SEL_EMIT(Min32F0x4, get_vtf(4), FCmpULT)
OPF0X_SEL_EMIT(Max64F0x2, get_vtd(2), FCmpUGT)
OPF0X_SEL_EMIT(Min64F0x2, get_vtd(2), FCmpULT)

OPF0X_RSQ(Sqrt64F0x2 , get_vtd(2), sqrt, NONE, F0XINT(get_d(), sqrt))
OPF0X_RSQ(Recip64F0x2, get_vtd(2), sqrt, NONE, F0XRCP(DBL_1))
OPF0X_RSQ(RSqrt64F0x2, get_vtd(2), sqrt, F0XINT(get_d(), sqrt), F0XRCP(DBL_1))

OPF0X_RSQ(Sqrt32F0x4 , get_vtf(4), sqrt, NONE, F0XINT(get_f(), sqrt))
OPF0X_RSQ(Recip32F0x4, get_vtf(4), sqrt, NONE, F0XRCP(FLT_1))
OPF0X_RSQ(RSqrt32F0x4, get_vtf(4), sqrt, F0XINT(get_f(), sqrt), F0XRCP(FLT_1))

EMIT_HELPER_BINOP(CmpF64, "vexop_cmpf64", get_d())

OPV_CMP_T_EMIT(CmpEQ32Fx2, get_vtf(2), FCmpOEQ, get_vt(2, 32))
OPV_CMP_T_EMIT(CmpEQ32Fx4, get_vtf(4), FCmpOEQ, get_vt(4, 32))
OPV_CMP_T_EMIT(CmpEQ64Fx2, get_vtd(2), FCmpOEQ, get_vt(2, 64))

OPV_CMP_T_EMIT(CmpGT32Fx2, get_vtf(2), FCmpOGT, get_vt(2, 32))
OPV_CMP_T_EMIT(CmpGT32Fx4, get_vtf(4), FCmpOGT, get_vt(4, 32))

OPV_CMP_T_EMIT(CmpGE32Fx2, get_vtf(2), FCmpOGE, get_vt(2, 32))
OPV_CMP_T_EMIT(CmpGE32Fx4, get_vtf(4), FCmpOGE, get_vt(4, 32))

OPV_CMP_T_EMIT(CmpLT32Fx4, get_vtf(4), FCmpOLT, get_vt(4, 32))
OPV_CMP_T_EMIT(CmpLT64Fx2, get_vtd(2), FCmpOLT, get_vt(2, 64))

OPV_CMP_T_EMIT(CmpLE32Fx4, get_vtf(4), FCmpOLE, get_vt(4, 32))
OPV_CMP_T_EMIT(CmpLE64Fx2, get_vtd(2), FCmpOLE, get_vt(2, 64))

OPV_CMP_T_EMIT(CmpUN32Fx4, get_vtf(4), FCmpUNO, get_vt(4, 32))
OPV_CMP_T_EMIT(CmpUN64Fx2, get_vtd(2), FCmpUNO, get_vt(2, 64))

OPV_EMIT(Add32Fx2 , get_vtf(2), FAdd )
OPV_EMIT(Add32Fx4 , get_vtf(4), FAdd )
OPV_EMIT(Add64Fx2 , get_vtd(2), FAdd )

OPV_EMIT(Sub32Fx2 , get_vtf(2), FSub )
OPV_EMIT(Sub32Fx4 , get_vtf(4), FSub )
OPV_EMIT(Sub64Fx2 , get_vtd(2), FSub )

OPV_EMIT(Mul32Fx2 , get_vtf(2), FMul )
OPV_EMIT(Mul32Fx4 , get_vtf(4), FMul )
OPV_EMIT(Mul64Fx2 , get_vtd(2), FMul )

OPV_EMIT(Div32Fx4 , get_vtf(4), FDiv )
OPV_EMIT(Div64Fx2 , get_vtd(2), FDiv )

UNOP_NEG_EMIT(NegF32, get_f(), FLT_0)
UNOP_NEG_EMIT(NegF64, get_d(), DBL_0)
UNOP_NEG_EMIT(Neg32Fx2, get_vtf(2), get_cv(g_flt_0x2))
UNOP_NEG_EMIT(Neg32Fx4, get_vtf(4), get_cv(g_flt_0x4))

TRIOP_EXPAND_EMIT(AddF64, BitCast, get_d(), FAdd)
TRIOP_EXPAND_EMIT(SubF64, BitCast, get_d(), FSub)
TRIOP_EXPAND_EMIT(DivF64, BitCast, get_d(), FDiv)
TRIOP_EXPAND_EMIT(MulF64, BitCast, get_d(), FMul)

TRIOP_EXPAND_EMIT(AddF32, BitCast, get_f(), FAdd)
TRIOP_EXPAND_EMIT(SubF32, BitCast, get_f(), FSub)
TRIOP_EXPAND_EMIT(DivF32, BitCast, get_f(), FDiv)
TRIOP_EXPAND_EMIT(MulF32, BitCast, get_f(), FMul)

Value* VexExprBinopRoundF32toInt::emit(void) const
{
	/* TODO: rounding mode */
	BINOP_SETUP
	v2 = builder->CreateBitCast(v2, get_f());
	return builder->CreateFSub(v2,
		builder->CreateFRem(v2, FLT_1));
}

Value* VexExprBinopRoundF64toInt::emit(void) const
{
	/* TODO: rounding mode */
	BINOP_SETUP
	v2 = builder->CreateBitCast(v2, get_d());
	return builder->CreateFSub(v2,
		builder->CreateFRem(v2, DBL_1));
}

Value* VexExprTriopPRemF64::emit(void) const
{
	/* ignoring rounding mode etc */
	TRIOP_SETUP
	v2 = builder->CreateBitCast(v2, get_d());
	v3 = builder->CreateBitCast(v3, get_d());
	return builder->CreateFRem(v2, v3);
}
Value* VexExprTriopPRemC3210F64::emit(void) const
{
	/* aside from the exception codes:
	C0 = bit2 of the quotient (Q2) 
	C1 = bit0 of the quotient (Q0) 
	C3 = bit1 of the quotient (Q1)
	*/
	TRIOP_SETUP
	v2 = builder->CreateBitCast(v2, get_d());
	v3 = builder->CreateBitCast(v3, get_d());
	Value *quotient = builder->CreateFDiv(v2, v3);
	quotient = builder->CreateFPToSI(quotient, get_i(32));
	Value *result = get_32i(0);
	result = builder->CreateOr(
		result,
		builder->CreateSelect(
			builder->CreateTrunc(quotient, get_i(1)),
			get_32i(0x200), get_32i(0)));
	result = builder->CreateOr(
		result,
		builder->CreateSelect(
			builder->CreateTrunc(
				builder->CreateLShr(quotient, get_32i(1)),
				get_i(1)),
			get_32i(0x4000), get_32i(0)));
	result = builder->CreateOr(
		result,
		builder->CreateSelect(
			builder->CreateTrunc(
				builder->CreateLShr(quotient, get_32i(2)),
				get_i(1)),
			get_32i(0x100), get_32i(0)));
	return result;
}

OPV_RSQ(Sqrt64Fx2 , get_vtd(2), sqrt, NONE, VINT(get_vtd(2), sqrt))
OPV_RSQ(Recip64Fx2, get_vtd(2), sqrt, NONE, VRCP(DBL_1x2))
OPV_RSQ(RSqrt64Fx2, get_vtd(2), sqrt, VINT(get_vtd(2), sqrt), VRCP(DBL_1x2))

OPV_RSQ(Sqrt32Fx4 , get_vtf(4), sqrt, NONE, VINT(get_vtf(4), sqrt))
OPV_RSQ(Recip32Fx4, get_vtf(4), sqrt, NONE, VRCP(FLT_1x4))
OPV_RSQ(RSqrt32Fx4, get_vtf(4), sqrt, VINT(get_vtf(4), sqrt), VRCP(FLT_1x4))

// UNOP_TAGOP(RSqrt32Fx4);
// UNOP_TAGOP(RSqrt64Fx2);
// 
// UNOP_TAGOP(Recip32Fx4);
// UNOP_TAGOP(Recip64Fx2);
//

Value* VexExprQopMAddF64::emit(void) const
{
	QOP_SETUP
	return builder->CreateFAdd(builder->CreateFMul(v2, v3), v4);
}
