#include <stdio.h>
#include <iostream>
#include "genllvm.h"
#include "vexop.h"
#include "vexhelpers.h"

using namespace llvm;

#define CASE_OP(x)	case Iop_##x: return "Iop_"#x;

#define OP_32_64(x)	\
	case Iop_##x##32 : 	\
	return "Iop_" #x "32";	\
	case Iop_##x##64 : 	\
	return "Iop_" #x "64";	\

#define OP_FULLRANGE(x)		\
	case Iop_##x##8 : 	\
	return "Iop_" #x "8";	\
	case Iop_##x##16 : 	\
	return "Iop_" #x "16";	\
	case Iop_##x##32 : 	\
	return "Iop_" #x "32";	\
	case Iop_##x##64 : 	\
	return "Iop_" #x "64";	\

const char* getVexOpName(IROp op)
{
	switch(op) {
	CASE_OP(SqrtF64r32)
	OP_32_64(SqrtF)
	OP_32_64(NegF)
	OP_32_64(AbsF)

	CASE_OP(AddF64r32)
	CASE_OP(SubF64r32)
	CASE_OP(MulF64r32)
	CASE_OP(DivF64r32)


	OP_FULLRANGE(Add)
	OP_FULLRANGE(Sub)
	OP_FULLRANGE(Mul)
	OP_FULLRANGE(Or)
	OP_FULLRANGE(And)
	OP_FULLRANGE(Xor)
	OP_FULLRANGE(Shl)
	OP_FULLRANGE(Shr)
	OP_FULLRANGE(Sar)
	OP_FULLRANGE(CmpEQ)
	OP_FULLRANGE(CmpNE)
	OP_FULLRANGE(Not)
	OP_FULLRANGE(CasCmpEQ)
	OP_FULLRANGE(CasCmpNE)
	OP_FULLRANGE(MullS)
	OP_FULLRANGE(MullU)
	/* Wierdo integer stuff */
	CASE_OP(Clz64)
	CASE_OP(Clz32)	/* count leading zeroes */
	CASE_OP(Ctz64)
	CASE_OP(Ctz32)	/* count trailing zeros */
      /* Ctz64/Ctz32/Clz64/Clz32 are UNDEFINED when given arguments of
         zero.  You must ensure they are never given a zero argument.
      */

      /* Standard integer comparisons */
CASE_OP(CmpLT32S)
CASE_OP(CmpLT64S)
CASE_OP(CmpLE32S)
CASE_OP(CmpLE64S)
CASE_OP(CmpLT32U)
CASE_OP(CmpLT64U)
CASE_OP(CmpLE32U)
CASE_OP(CmpLE64U)
OP_FULLRANGE(CmpNEZ)
OP_32_64(CmpwNEZ)
OP_FULLRANGE(Left)
CASE_OP(Max32U)

      /* PowerPC-style 3-way integer comparisons.  Without them it is
         difficult to simulate PPC efficiently.
         op(x,y) | x < y  = 0x8 else 
                 | x > y  = 0x4 else
                 | x == y = 0x2
      */
CASE_OP(CmpORD32U)
CASE_OP(CmpORD64U)
CASE_OP(CmpORD32S)
CASE_OP(CmpORD64S)

OP_32_64(DivU)
OP_32_64(DivS)

CASE_OP(DivModU64to32)
CASE_OP(DivModS64to32)
CASE_OP(DivModU128to64)
CASE_OP(DivModS128to64)

/* Integer conversions.  Some of these are redundant (eg
 Iop_64to8 is the same as Iop_64to32 and then Iop_32to8), but
 having a complete set reduces the typical dynamic size of IR
 and makes the instruction selectors easier to write. */

/* Widening conversions */
CASE_OP(8Uto16)
CASE_OP(8Uto32)
CASE_OP(8Uto64)

CASE_OP(16Uto32)
CASE_OP(16Uto64)
CASE_OP(32Uto64)

CASE_OP(8Sto16)
CASE_OP(8Sto32)
CASE_OP(8Sto64)

CASE_OP(16Sto32)
CASE_OP(16Sto64)
CASE_OP(32Sto64)

/* Narrowing conversions */
CASE_OP(64to8)
CASE_OP(32to8)
CASE_OP(64to16)

/* 8 <-> 16 bit conversions */
CASE_OP(16to8)		// :: I16 -> I8, low half
CASE_OP(16HIto8)	// :: I16 -> I8, high half
CASE_OP(8HLto16)	// :: (I8,I8) -> I16
/* 16 <-> 32 bit conversions */
CASE_OP(32to16)		// :: I32 -> I16, low half
CASE_OP(32HIto16)	// :: I32 -> I16, high half
CASE_OP(16HLto32)	// :: (I16,I16) -> I32
/* 32 <-> 64 bit conversions */
CASE_OP(64to32)		// :: I64 -> I32, low half
CASE_OP(64HIto32)	// :: I64 -> I32, high half
CASE_OP(32HLto64)	// :: (I32,I32) -> I64
/* 64 <-> 128 bit conversions */
CASE_OP(128to64)	// :: I128 -> I64, low half
CASE_OP(128HIto64)	// :: I128 -> I64, high half
CASE_OP(64HLto128)	// :: (I64,I64) -> I128
/* 1-bit stuff */
CASE_OP(Not1)	/* :: Ity_Bit -> Ity_Bit */
CASE_OP(32to1)	/* :: Ity_I32 -> Ity_Bit, just select bit[0] */
CASE_OP(64to1)	/* :: Ity_I64 -> Ity_Bit, just select bit[0] */
CASE_OP(1Uto8)	/* :: Ity_Bit -> Ity_I8,  unsigned widen */
CASE_OP(1Uto32)	/* :: Ity_Bit -> Ity_I32, unsigned widen */
CASE_OP(1Uto64)	/* :: Ity_Bit -> Ity_I64, unsigned widen */
CASE_OP(1Sto8)	/* :: Ity_Bit -> Ity_I8,  signed widen */
CASE_OP(1Sto16)	/* :: Ity_Bit -> Ity_I16, signed widen */
CASE_OP(1Sto32)	/* :: Ity_Bit -> Ity_I32, signed widen */
CASE_OP(1Sto64)	/* :: Ity_Bit -> Ity_I64, signed widen */

CASE_OP(32UtoV128)
CASE_OP(64HLtoV128)	// :: (I64,I64) -> I128
CASE_OP(V128to64)
CASE_OP(InterleaveLO8x16)
CASE_OP(AddF64)
CASE_OP(SubF64)
CASE_OP(MulF64)
CASE_OP(DivF64)
CASE_OP(AddF32)
CASE_OP(SubF32)
CASE_OP(MulF32)
CASE_OP(DivF32)
CASE_OP(CmpF64)

CASE_OP(Add64Fx2)
CASE_OP(Sub64Fx2)
CASE_OP(Mul64Fx2)
CASE_OP(Div64Fx2)
CASE_OP(Max64Fx2)
CASE_OP(Min64Fx2)
CASE_OP(CmpEQ64Fx2)
CASE_OP(CmpLT64Fx2)
CASE_OP(CmpLE64Fx2)
CASE_OP(CmpUN64Fx2)
CASE_OP(Recip64Fx2)
CASE_OP(Sqrt64Fx2)
CASE_OP(RSqrt64Fx2)
CASE_OP(Add64F0x2)
CASE_OP(Sub64F0x2)
CASE_OP(Mul64F0x2)
CASE_OP(Div64F0x2)
CASE_OP(Max64F0x2)
CASE_OP(Min64F0x2)
CASE_OP(CmpEQ64F0x2)
CASE_OP(CmpLT64F0x2)
CASE_OP(CmpLE64F0x2)
CASE_OP(CmpUN64F0x2)
CASE_OP(Recip64F0x2)
CASE_OP(Sqrt64F0x2)
CASE_OP(RSqrt64F0x2)

case Iop_F64toI16S: fprintf(stderr, "unimplemented Iop_F64toI16S (%x)\n", op); return "Iop_F64toI16S";
case Iop_F64toI32S: fprintf(stderr, "unimplemented Iop_F64toI32S (%x)\n", op); return "Iop_F64toI32S";
case Iop_F64toI64S: fprintf(stderr, "unimplemented Iop_F64toI64S (%x)\n", op); return "Iop_F64toI64S";
case Iop_F64toI32U: fprintf(stderr, "unimplemented Iop_F64toI32U (%x)\n", op); return "Iop_F64toI32U";
case Iop_I16StoF64: fprintf(stderr, "unimplemented Iop_I16StoF64 (%x)\n", op); return "Iop_I16StoF64";
case Iop_I32StoF64: fprintf(stderr, "unimplemented Iop_I32StoF64 (%x)\n", op); return "Iop_I32StoF64";
case Iop_I64StoF64: fprintf(stderr, "unimplemented Iop_I64StoF64 (%x)\n", op); return "Iop_I64StoF64";
case Iop_I32UtoF64: fprintf(stderr, "unimplemented Iop_I32UtoF64 (%x)\n", op); return "Iop_I32UtoF64";
case Iop_F32toF64: fprintf(stderr, "unimplemented Iop_F32toF64 (%x)\n", op); return "Iop_F32toF64";
case Iop_F64toF32: fprintf(stderr, "unimplemented Iop_F64toF32 (%x)\n", op); return "Iop_F64toF32";
case Iop_ReinterpF64asI64: fprintf(stderr, "unimplemented Iop_ReinterpF64asI64 (%x)\n", op); return "Iop_ReinterpF64asI64";
case Iop_ReinterpI64asF64: fprintf(stderr, "unimplemented Iop_ReinterpI64asF64 (%x)\n", op); return "Iop_ReinterpI64asF64";
case Iop_ReinterpF32asI32: fprintf(stderr, "unimplemented Iop_ReinterpF32asI32 (%x)\n", op); return "Iop_ReinterpF32asI32";
case Iop_ReinterpI32asF32: fprintf(stderr, "unimplemented Iop_ReinterpI32asF32 (%x)\n", op); return "Iop_ReinterpI32asF32";
case Iop_AtanF64: fprintf(stderr, "unimplemented Iop_AtanF64 (%x)\n", op); return "Iop_AtanF64";
case Iop_Yl2xF64: fprintf(stderr, "unimplemented Iop_Yl2xF64 (%x)\n", op); return "Iop_Yl2xF64";
case Iop_Yl2xp1F64: fprintf(stderr, "unimplemented Iop_Yl2xp1F64 (%x)\n", op); return "Iop_Yl2xp1F64";
case Iop_PRemF64: fprintf(stderr, "unimplemented Iop_PRemF64 (%x)\n", op); return "Iop_PRemF64";
case Iop_PRemC3210F64: fprintf(stderr, "unimplemented Iop_PRemC3210F64 (%x)\n", op); return "Iop_PRemC3210F64";
case Iop_PRem1F64: fprintf(stderr, "unimplemented Iop_PRem1F64 (%x)\n", op); return "Iop_PRem1F64";
case Iop_PRem1C3210F64: fprintf(stderr, "unimplemented Iop_PRem1C3210F64 (%x)\n", op); return "Iop_PRem1C3210F64";
case Iop_ScaleF64: fprintf(stderr, "unimplemented Iop_ScaleF64 (%x)\n", op); return "Iop_ScaleF64";
case Iop_SinF64: fprintf(stderr, "unimplemented Iop_SinF64 (%x)\n", op); return "Iop_SinF64";
case Iop_CosF64: fprintf(stderr, "unimplemented Iop_CosF64 (%x)\n", op); return "Iop_CosF64";
case Iop_TanF64: fprintf(stderr, "unimplemented Iop_TanF64 (%x)\n", op); return "Iop_TanF64";
case Iop_2xm1F64: fprintf(stderr, "unimplemented Iop_2xm1F64 (%x)\n", op); return "Iop_2xm1F64";
case Iop_RoundF64toInt: fprintf(stderr, "unimplemented Iop_RoundF64toInt (%x)\n", op); return "Iop_RoundF64toInt";
case Iop_RoundF32toInt: fprintf(stderr, "unimplemented Iop_RoundF32toInt (%x)\n", op); return "Iop_RoundF32toInt";
case Iop_MAddF64: fprintf(stderr, "unimplemented Iop_MAddF64 (%x)\n", op); return "Iop_MAddF64";
case Iop_MSubF64: fprintf(stderr, "unimplemented Iop_MSubF64 (%x)\n", op); return "Iop_MSubF64";
case Iop_MAddF64r32: fprintf(stderr, "unimplemented Iop_MAddF64r32 (%x)\n", op); return "Iop_MAddF64r32";
case Iop_MSubF64r32: fprintf(stderr, "unimplemented Iop_MSubF64r32 (%x)\n", op); return "Iop_MSubF64r32";
case Iop_Est5FRSqrt: fprintf(stderr, "unimplemented Iop_Est5FRSqrt (%x)\n", op); return "Iop_Est5FRSqrt";
case Iop_RoundF64toF64_NEAREST: fprintf(stderr, "unimplemented Iop_RoundF64toF64_NEAREST (%x)\n", op); return "Iop_RoundF64toF64_NEAREST";
case Iop_RoundF64toF64_NegINF: fprintf(stderr, "unimplemented Iop_RoundF64toF64_NegINF (%x)\n", op); return "Iop_RoundF64toF64_NegINF";
case Iop_RoundF64toF64_PosINF: fprintf(stderr, "unimplemented Iop_RoundF64toF64_PosINF (%x)\n", op); return "Iop_RoundF64toF64_PosINF";
case Iop_RoundF64toF64_ZERO: fprintf(stderr, "unimplemented Iop_RoundF64toF64_ZERO (%x)\n", op); return "Iop_RoundF64toF64_ZERO";
case Iop_TruncF64asF32: fprintf(stderr, "unimplemented Iop_TruncF64asF32 (%x)\n", op); return "Iop_TruncF64asF32";
case Iop_RoundF64toF32: fprintf(stderr, "unimplemented Iop_RoundF64toF32 (%x)\n", op); return "Iop_RoundF64toF32";
case Iop_CalcFPRF: fprintf(stderr, "unimplemented Iop_CalcFPRF (%x)\n", op); return "Iop_CalcFPRF";
case Iop_Add16x2: fprintf(stderr, "unimplemented Iop_Add16x2 (%x)\n", op); return "Iop_Add16x2";
case Iop_Sub16x2: fprintf(stderr, "unimplemented Iop_Sub16x2 (%x)\n", op); return "Iop_Sub16x2";
case Iop_QAdd16Sx2: fprintf(stderr, "unimplemented Iop_QAdd16Sx2 (%x)\n", op); return "Iop_QAdd16Sx2";
case Iop_QAdd16Ux2: fprintf(stderr, "unimplemented Iop_QAdd16Ux2 (%x)\n", op); return "Iop_QAdd16Ux2";
case Iop_QSub16Sx2: fprintf(stderr, "unimplemented Iop_QSub16Sx2 (%x)\n", op); return "Iop_QSub16Sx2";
case Iop_QSub16Ux2: fprintf(stderr, "unimplemented Iop_QSub16Ux2 (%x)\n", op); return "Iop_QSub16Ux2";
case Iop_HAdd16Ux2: fprintf(stderr, "unimplemented Iop_HAdd16Ux2 (%x)\n", op); return "Iop_HAdd16Ux2";
case Iop_HAdd16Sx2: fprintf(stderr, "unimplemented Iop_HAdd16Sx2 (%x)\n", op); return "Iop_HAdd16Sx2";
case Iop_HSub16Ux2: fprintf(stderr, "unimplemented Iop_HSub16Ux2 (%x)\n", op); return "Iop_HSub16Ux2";
case Iop_HSub16Sx2: fprintf(stderr, "unimplemented Iop_HSub16Sx2 (%x)\n", op); return "Iop_HSub16Sx2";
case Iop_Add8x4: fprintf(stderr, "unimplemented Iop_Add8x4 (%x)\n", op); return "Iop_Add8x4";
case Iop_Sub8x4: fprintf(stderr, "unimplemented Iop_Sub8x4 (%x)\n", op); return "Iop_Sub8x4";
case Iop_QAdd8Sx4: fprintf(stderr, "unimplemented Iop_QAdd8Sx4 (%x)\n", op); return "Iop_QAdd8Sx4";
case Iop_QAdd8Ux4: fprintf(stderr, "unimplemented Iop_QAdd8Ux4 (%x)\n", op); return "Iop_QAdd8Ux4";
case Iop_QSub8Sx4: fprintf(stderr, "unimplemented Iop_QSub8Sx4 (%x)\n", op); return "Iop_QSub8Sx4";
case Iop_QSub8Ux4: fprintf(stderr, "unimplemented Iop_QSub8Ux4 (%x)\n", op); return "Iop_QSub8Ux4";
case Iop_HAdd8Ux4: fprintf(stderr, "unimplemented Iop_HAdd8Ux4 (%x)\n", op); return "Iop_HAdd8Ux4";
case Iop_HAdd8Sx4: fprintf(stderr, "unimplemented Iop_HAdd8Sx4 (%x)\n", op); return "Iop_HAdd8Sx4";
case Iop_HSub8Ux4: fprintf(stderr, "unimplemented Iop_HSub8Ux4 (%x)\n", op); return "Iop_HSub8Ux4";
case Iop_HSub8Sx4: fprintf(stderr, "unimplemented Iop_HSub8Sx4 (%x)\n", op); return "Iop_HSub8Sx4";
case Iop_Sad8Ux4: fprintf(stderr, "unimplemented Iop_Sad8Ux4 (%x)\n", op); return "Iop_Sad8Ux4";
case Iop_CmpNEZ16x2: fprintf(stderr, "unimplemented Iop_CmpNEZ16x2 (%x)\n", op); return "Iop_CmpNEZ16x2";
case Iop_CmpNEZ8x4: fprintf(stderr, "unimplemented Iop_CmpNEZ8x4 (%x)\n", op); return "Iop_CmpNEZ8x4";
case Iop_I32UtoFx2: fprintf(stderr, "unimplemented Iop_I32UtoFx2 (%x)\n", op); return "Iop_I32UtoFx2";
case Iop_I32StoFx2: fprintf(stderr, "unimplemented Iop_I32StoFx2 (%x)\n", op); return "Iop_I32StoFx2";
case Iop_FtoI32Ux2_RZ: fprintf(stderr, "unimplemented Iop_FtoI32Ux2_RZ (%x)\n", op); return "Iop_FtoI32Ux2_RZ";
case Iop_FtoI32Sx2_RZ: fprintf(stderr, "unimplemented Iop_FtoI32Sx2_RZ (%x)\n", op); return "Iop_FtoI32Sx2_RZ";
case Iop_F32ToFixed32Ux2_RZ: fprintf(stderr, "unimplemented Iop_F32ToFixed32Ux2_RZ (%x)\n", op); return "Iop_F32ToFixed32Ux2_RZ";
case Iop_F32ToFixed32Sx2_RZ: fprintf(stderr, "unimplemented Iop_F32ToFixed32Sx2_RZ (%x)\n", op); return "Iop_F32ToFixed32Sx2_RZ";
case Iop_Fixed32UToF32x2_RN: fprintf(stderr, "unimplemented Iop_Fixed32UToF32x2_RN (%x)\n", op); return "Iop_Fixed32UToF32x2_RN";
case Iop_Fixed32SToF32x2_RN: fprintf(stderr, "unimplemented Iop_Fixed32SToF32x2_RN (%x)\n", op); return "Iop_Fixed32SToF32x2_RN";
case Iop_Max32Fx2: fprintf(stderr, "unimplemented Iop_Max32Fx2 (%x)\n", op); return "Iop_Max32Fx2";
case Iop_Min32Fx2: fprintf(stderr, "unimplemented Iop_Min32Fx2 (%x)\n", op); return "Iop_Min32Fx2";
case Iop_PwMax32Fx2: fprintf(stderr, "unimplemented Iop_PwMax32Fx2 (%x)\n", op); return "Iop_PwMax32Fx2";
case Iop_PwMin32Fx2: fprintf(stderr, "unimplemented Iop_PwMin32Fx2 (%x)\n", op); return "Iop_PwMin32Fx2";
case Iop_CmpEQ32Fx2: fprintf(stderr, "unimplemented Iop_CmpEQ32Fx2 (%x)\n", op); return "Iop_CmpEQ32Fx2";
case Iop_CmpGT32Fx2: fprintf(stderr, "unimplemented Iop_CmpGT32Fx2 (%x)\n", op); return "Iop_CmpGT32Fx2";
case Iop_CmpGE32Fx2: fprintf(stderr, "unimplemented Iop_CmpGE32Fx2 (%x)\n", op); return "Iop_CmpGE32Fx2";
case Iop_Recip32Fx2: fprintf(stderr, "unimplemented Iop_Recip32Fx2 (%x)\n", op); return "Iop_Recip32Fx2";
case Iop_Recps32Fx2: fprintf(stderr, "unimplemented Iop_Recps32Fx2 (%x)\n", op); return "Iop_Recps32Fx2";
case Iop_Rsqrte32Fx2: fprintf(stderr, "unimplemented Iop_Rsqrte32Fx2 (%x)\n", op); return "Iop_Rsqrte32Fx2";
case Iop_Rsqrts32Fx2: fprintf(stderr, "unimplemented Iop_Rsqrts32Fx2 (%x)\n", op); return "Iop_Rsqrts32Fx2";
case Iop_Neg32Fx2: fprintf(stderr, "unimplemented Iop_Neg32Fx2 (%x)\n", op); return "Iop_Neg32Fx2";
case Iop_Abs32Fx2: fprintf(stderr, "unimplemented Iop_Abs32Fx2 (%x)\n", op); return "Iop_Abs32Fx2";
case Iop_CmpNEZ8x8: fprintf(stderr, "unimplemented Iop_CmpNEZ8x8 (%x)\n", op); return "Iop_CmpNEZ8x8";
case Iop_CmpNEZ16x4: fprintf(stderr, "unimplemented Iop_CmpNEZ16x4 (%x)\n", op); return "Iop_CmpNEZ16x4";
case Iop_CmpNEZ32x2: fprintf(stderr, "unimplemented Iop_CmpNEZ32x2 (%x)\n", op); return "Iop_CmpNEZ32x2";
case Iop_Add8x8: fprintf(stderr, "unimplemented Iop_Add8x8 (%x)\n", op); return "Iop_Add8x8";
case Iop_Add16x4: fprintf(stderr, "unimplemented Iop_Add16x4 (%x)\n", op); return "Iop_Add16x4";
case Iop_Add32x2: fprintf(stderr, "unimplemented Iop_Add32x2 (%x)\n", op); return "Iop_Add32x2";
case Iop_QAdd8Ux8: fprintf(stderr, "unimplemented Iop_QAdd8Ux8 (%x)\n", op); return "Iop_QAdd8Ux8";
case Iop_QAdd16Ux4: fprintf(stderr, "unimplemented Iop_QAdd16Ux4 (%x)\n", op); return "Iop_QAdd16Ux4";
case Iop_QAdd32Ux2: fprintf(stderr, "unimplemented Iop_QAdd32Ux2 (%x)\n", op); return "Iop_QAdd32Ux2";
case Iop_QAdd64Ux1: fprintf(stderr, "unimplemented Iop_QAdd64Ux1 (%x)\n", op); return "Iop_QAdd64Ux1";
case Iop_QAdd8Sx8: fprintf(stderr, "unimplemented Iop_QAdd8Sx8 (%x)\n", op); return "Iop_QAdd8Sx8";
case Iop_QAdd16Sx4: fprintf(stderr, "unimplemented Iop_QAdd16Sx4 (%x)\n", op); return "Iop_QAdd16Sx4";
case Iop_QAdd32Sx2: fprintf(stderr, "unimplemented Iop_QAdd32Sx2 (%x)\n", op); return "Iop_QAdd32Sx2";
case Iop_QAdd64Sx1: fprintf(stderr, "unimplemented Iop_QAdd64Sx1 (%x)\n", op); return "Iop_QAdd64Sx1";
case Iop_PwAdd8x8: fprintf(stderr, "unimplemented Iop_PwAdd8x8 (%x)\n", op); return "Iop_PwAdd8x8";
case Iop_PwAdd16x4: fprintf(stderr, "unimplemented Iop_PwAdd16x4 (%x)\n", op); return "Iop_PwAdd16x4";
case Iop_PwAdd32x2: fprintf(stderr, "unimplemented Iop_PwAdd32x2 (%x)\n", op); return "Iop_PwAdd32x2";
case Iop_PwMax8Sx8: fprintf(stderr, "unimplemented Iop_PwMax8Sx8 (%x)\n", op); return "Iop_PwMax8Sx8";
case Iop_PwMax16Sx4: fprintf(stderr, "unimplemented Iop_PwMax16Sx4 (%x)\n", op); return "Iop_PwMax16Sx4";
case Iop_PwMax32Sx2: fprintf(stderr, "unimplemented Iop_PwMax32Sx2 (%x)\n", op); return "Iop_PwMax32Sx2";
case Iop_PwMax8Ux8: fprintf(stderr, "unimplemented Iop_PwMax8Ux8 (%x)\n", op); return "Iop_PwMax8Ux8";
case Iop_PwMax16Ux4: fprintf(stderr, "unimplemented Iop_PwMax16Ux4 (%x)\n", op); return "Iop_PwMax16Ux4";
case Iop_PwMax32Ux2: fprintf(stderr, "unimplemented Iop_PwMax32Ux2 (%x)\n", op); return "Iop_PwMax32Ux2";
case Iop_PwMin8Sx8: fprintf(stderr, "unimplemented Iop_PwMin8Sx8 (%x)\n", op); return "Iop_PwMin8Sx8";
case Iop_PwMin16Sx4: fprintf(stderr, "unimplemented Iop_PwMin16Sx4 (%x)\n", op); return "Iop_PwMin16Sx4";
case Iop_PwMin32Sx2: fprintf(stderr, "unimplemented Iop_PwMin32Sx2 (%x)\n", op); return "Iop_PwMin32Sx2";
case Iop_PwMin8Ux8: fprintf(stderr, "unimplemented Iop_PwMin8Ux8 (%x)\n", op); return "Iop_PwMin8Ux8";
case Iop_PwMin16Ux4: fprintf(stderr, "unimplemented Iop_PwMin16Ux4 (%x)\n", op); return "Iop_PwMin16Ux4";
case Iop_PwMin32Ux2: fprintf(stderr, "unimplemented Iop_PwMin32Ux2 (%x)\n", op); return "Iop_PwMin32Ux2";
case Iop_PwAddL8Ux8: fprintf(stderr, "unimplemented Iop_PwAddL8Ux8 (%x)\n", op); return "Iop_PwAddL8Ux8";
case Iop_PwAddL16Ux4: fprintf(stderr, "unimplemented Iop_PwAddL16Ux4 (%x)\n", op); return "Iop_PwAddL16Ux4";
case Iop_PwAddL32Ux2: fprintf(stderr, "unimplemented Iop_PwAddL32Ux2 (%x)\n", op); return "Iop_PwAddL32Ux2";
case Iop_PwAddL8Sx8: fprintf(stderr, "unimplemented Iop_PwAddL8Sx8 (%x)\n", op); return "Iop_PwAddL8Sx8";
case Iop_PwAddL16Sx4: fprintf(stderr, "unimplemented Iop_PwAddL16Sx4 (%x)\n", op); return "Iop_PwAddL16Sx4";
case Iop_PwAddL32Sx2: fprintf(stderr, "unimplemented Iop_PwAddL32Sx2 (%x)\n", op); return "Iop_PwAddL32Sx2";
case Iop_Sub8x8: fprintf(stderr, "unimplemented Iop_Sub8x8 (%x)\n", op); return "Iop_Sub8x8";
case Iop_Sub16x4: fprintf(stderr, "unimplemented Iop_Sub16x4 (%x)\n", op); return "Iop_Sub16x4";
case Iop_Sub32x2: fprintf(stderr, "unimplemented Iop_Sub32x2 (%x)\n", op); return "Iop_Sub32x2";
case Iop_QSub8Ux8: fprintf(stderr, "unimplemented Iop_QSub8Ux8 (%x)\n", op); return "Iop_QSub8Ux8";
case Iop_QSub16Ux4: fprintf(stderr, "unimplemented Iop_QSub16Ux4 (%x)\n", op); return "Iop_QSub16Ux4";
case Iop_QSub32Ux2: fprintf(stderr, "unimplemented Iop_QSub32Ux2 (%x)\n", op); return "Iop_QSub32Ux2";
case Iop_QSub64Ux1: fprintf(stderr, "unimplemented Iop_QSub64Ux1 (%x)\n", op); return "Iop_QSub64Ux1";
case Iop_QSub8Sx8: fprintf(stderr, "unimplemented Iop_QSub8Sx8 (%x)\n", op); return "Iop_QSub8Sx8";
case Iop_QSub16Sx4: fprintf(stderr, "unimplemented Iop_QSub16Sx4 (%x)\n", op); return "Iop_QSub16Sx4";
case Iop_QSub32Sx2: fprintf(stderr, "unimplemented Iop_QSub32Sx2 (%x)\n", op); return "Iop_QSub32Sx2";
case Iop_QSub64Sx1: fprintf(stderr, "unimplemented Iop_QSub64Sx1 (%x)\n", op); return "Iop_QSub64Sx1";
case Iop_Abs8x8: fprintf(stderr, "unimplemented Iop_Abs8x8 (%x)\n", op); return "Iop_Abs8x8";
case Iop_Abs16x4: fprintf(stderr, "unimplemented Iop_Abs16x4 (%x)\n", op); return "Iop_Abs16x4";
case Iop_Abs32x2: fprintf(stderr, "unimplemented Iop_Abs32x2 (%x)\n", op); return "Iop_Abs32x2";
case Iop_Mul8x8: fprintf(stderr, "unimplemented Iop_Mul8x8 (%x)\n", op); return "Iop_Mul8x8";
case Iop_Mul16x4: fprintf(stderr, "unimplemented Iop_Mul16x4 (%x)\n", op); return "Iop_Mul16x4";
case Iop_Mul32x2: fprintf(stderr, "unimplemented Iop_Mul32x2 (%x)\n", op); return "Iop_Mul32x2";
case Iop_Mul32Fx2: fprintf(stderr, "unimplemented Iop_Mul32Fx2 (%x)\n", op); return "Iop_Mul32Fx2";
case Iop_MulHi16Ux4: fprintf(stderr, "unimplemented Iop_MulHi16Ux4 (%x)\n", op); return "Iop_MulHi16Ux4";
case Iop_MulHi16Sx4: fprintf(stderr, "unimplemented Iop_MulHi16Sx4 (%x)\n", op); return "Iop_MulHi16Sx4";
case Iop_PolynomialMul8x8: fprintf(stderr, "unimplemented Iop_PolynomialMul8x8 (%x)\n", op); return "Iop_PolynomialMul8x8";
case Iop_QDMulHi16Sx4: fprintf(stderr, "unimplemented Iop_QDMulHi16Sx4 (%x)\n", op); return "Iop_QDMulHi16Sx4";
case Iop_QDMulHi32Sx2: fprintf(stderr, "unimplemented Iop_QDMulHi32Sx2 (%x)\n", op); return "Iop_QDMulHi32Sx2";
case Iop_QRDMulHi16Sx4: fprintf(stderr, "unimplemented Iop_QRDMulHi16Sx4 (%x)\n", op); return "Iop_QRDMulHi16Sx4";
case Iop_QRDMulHi32Sx2: fprintf(stderr, "unimplemented Iop_QRDMulHi32Sx2 (%x)\n", op); return "Iop_QRDMulHi32Sx2";
case Iop_Avg8Ux8: fprintf(stderr, "unimplemented Iop_Avg8Ux8 (%x)\n", op); return "Iop_Avg8Ux8";
case Iop_Avg16Ux4: fprintf(stderr, "unimplemented Iop_Avg16Ux4 (%x)\n", op); return "Iop_Avg16Ux4";
case Iop_Max8Sx8: fprintf(stderr, "unimplemented Iop_Max8Sx8 (%x)\n", op); return "Iop_Max8Sx8";
case Iop_Max16Sx4: fprintf(stderr, "unimplemented Iop_Max16Sx4 (%x)\n", op); return "Iop_Max16Sx4";
case Iop_Max32Sx2: fprintf(stderr, "unimplemented Iop_Max32Sx2 (%x)\n", op); return "Iop_Max32Sx2";
case Iop_Max8Ux8: fprintf(stderr, "unimplemented Iop_Max8Ux8 (%x)\n", op); return "Iop_Max8Ux8";
case Iop_Max16Ux4: fprintf(stderr, "unimplemented Iop_Max16Ux4 (%x)\n", op); return "Iop_Max16Ux4";
case Iop_Max32Ux2: fprintf(stderr, "unimplemented Iop_Max32Ux2 (%x)\n", op); return "Iop_Max32Ux2";
case Iop_Min8Sx8: fprintf(stderr, "unimplemented Iop_Min8Sx8 (%x)\n", op); return "Iop_Min8Sx8";
case Iop_Min16Sx4: fprintf(stderr, "unimplemented Iop_Min16Sx4 (%x)\n", op); return "Iop_Min16Sx4";
case Iop_Min32Sx2: fprintf(stderr, "unimplemented Iop_Min32Sx2 (%x)\n", op); return "Iop_Min32Sx2";
case Iop_Min8Ux8: fprintf(stderr, "unimplemented Iop_Min8Ux8 (%x)\n", op); return "Iop_Min8Ux8";
case Iop_Min16Ux4: fprintf(stderr, "unimplemented Iop_Min16Ux4 (%x)\n", op); return "Iop_Min16Ux4";
case Iop_Min32Ux2: fprintf(stderr, "unimplemented Iop_Min32Ux2 (%x)\n", op); return "Iop_Min32Ux2";
case Iop_CmpEQ8x8: fprintf(stderr, "unimplemented Iop_CmpEQ8x8 (%x)\n", op); return "Iop_CmpEQ8x8";
case Iop_CmpEQ16x4: fprintf(stderr, "unimplemented Iop_CmpEQ16x4 (%x)\n", op); return "Iop_CmpEQ16x4";
case Iop_CmpEQ32x2: fprintf(stderr, "unimplemented Iop_CmpEQ32x2 (%x)\n", op); return "Iop_CmpEQ32x2";
case Iop_CmpGT8Ux8: fprintf(stderr, "unimplemented Iop_CmpGT8Ux8 (%x)\n", op); return "Iop_CmpGT8Ux8";
case Iop_CmpGT16Ux4: fprintf(stderr, "unimplemented Iop_CmpGT16Ux4 (%x)\n", op); return "Iop_CmpGT16Ux4";
case Iop_CmpGT32Ux2: fprintf(stderr, "unimplemented Iop_CmpGT32Ux2 (%x)\n", op); return "Iop_CmpGT32Ux2";
case Iop_CmpGT8Sx8: fprintf(stderr, "unimplemented Iop_CmpGT8Sx8 (%x)\n", op); return "Iop_CmpGT8Sx8";
case Iop_CmpGT16Sx4: fprintf(stderr, "unimplemented Iop_CmpGT16Sx4 (%x)\n", op); return "Iop_CmpGT16Sx4";
case Iop_CmpGT32Sx2: fprintf(stderr, "unimplemented Iop_CmpGT32Sx2 (%x)\n", op); return "Iop_CmpGT32Sx2";
case Iop_Cnt8x8: fprintf(stderr, "unimplemented Iop_Cnt8x8 (%x)\n", op); return "Iop_Cnt8x8";
case Iop_Clz8Sx8: fprintf(stderr, "unimplemented Iop_Clz8Sx8 (%x)\n", op); return "Iop_Clz8Sx8";
case Iop_Clz16Sx4: fprintf(stderr, "unimplemented Iop_Clz16Sx4 (%x)\n", op); return "Iop_Clz16Sx4";
case Iop_Clz32Sx2: fprintf(stderr, "unimplemented Iop_Clz32Sx2 (%x)\n", op); return "Iop_Clz32Sx2";
case Iop_Cls8Sx8: fprintf(stderr, "unimplemented Iop_Cls8Sx8 (%x)\n", op); return "Iop_Cls8Sx8";
case Iop_Cls16Sx4: fprintf(stderr, "unimplemented Iop_Cls16Sx4 (%x)\n", op); return "Iop_Cls16Sx4";
case Iop_Cls32Sx2: fprintf(stderr, "unimplemented Iop_Cls32Sx2 (%x)\n", op); return "Iop_Cls32Sx2";
case Iop_Shl8x8: fprintf(stderr, "unimplemented Iop_Shl8x8 (%x)\n", op); return "Iop_Shl8x8";
case Iop_Shl16x4: fprintf(stderr, "unimplemented Iop_Shl16x4 (%x)\n", op); return "Iop_Shl16x4";
case Iop_Shl32x2: fprintf(stderr, "unimplemented Iop_Shl32x2 (%x)\n", op); return "Iop_Shl32x2";
case Iop_Shr8x8: fprintf(stderr, "unimplemented Iop_Shr8x8 (%x)\n", op); return "Iop_Shr8x8";
case Iop_Shr16x4: fprintf(stderr, "unimplemented Iop_Shr16x4 (%x)\n", op); return "Iop_Shr16x4";
case Iop_Shr32x2: fprintf(stderr, "unimplemented Iop_Shr32x2 (%x)\n", op); return "Iop_Shr32x2";
case Iop_Sar8x8: fprintf(stderr, "unimplemented Iop_Sar8x8 (%x)\n", op); return "Iop_Sar8x8";
case Iop_Sar16x4: fprintf(stderr, "unimplemented Iop_Sar16x4 (%x)\n", op); return "Iop_Sar16x4";
case Iop_Sar32x2: fprintf(stderr, "unimplemented Iop_Sar32x2 (%x)\n", op); return "Iop_Sar32x2";
case Iop_Sal8x8: fprintf(stderr, "unimplemented Iop_Sal8x8 (%x)\n", op); return "Iop_Sal8x8";
case Iop_Sal16x4: fprintf(stderr, "unimplemented Iop_Sal16x4 (%x)\n", op); return "Iop_Sal16x4";
case Iop_Sal32x2: fprintf(stderr, "unimplemented Iop_Sal32x2 (%x)\n", op); return "Iop_Sal32x2";
case Iop_Sal64x1: fprintf(stderr, "unimplemented Iop_Sal64x1 (%x)\n", op); return "Iop_Sal64x1";
case Iop_ShlN8x8: fprintf(stderr, "unimplemented Iop_ShlN8x8 (%x)\n", op); return "Iop_ShlN8x8";
case Iop_ShlN16x4: fprintf(stderr, "unimplemented Iop_ShlN16x4 (%x)\n", op); return "Iop_ShlN16x4";
case Iop_ShlN32x2: fprintf(stderr, "unimplemented Iop_ShlN32x2 (%x)\n", op); return "Iop_ShlN32x2";
case Iop_ShrN8x8: fprintf(stderr, "unimplemented Iop_ShrN8x8 (%x)\n", op); return "Iop_ShrN8x8";
case Iop_ShrN16x4: fprintf(stderr, "unimplemented Iop_ShrN16x4 (%x)\n", op); return "Iop_ShrN16x4";
case Iop_ShrN32x2: fprintf(stderr, "unimplemented Iop_ShrN32x2 (%x)\n", op); return "Iop_ShrN32x2";
case Iop_SarN8x8: fprintf(stderr, "unimplemented Iop_SarN8x8 (%x)\n", op); return "Iop_SarN8x8";
case Iop_SarN16x4: fprintf(stderr, "unimplemented Iop_SarN16x4 (%x)\n", op); return "Iop_SarN16x4";
case Iop_SarN32x2: fprintf(stderr, "unimplemented Iop_SarN32x2 (%x)\n", op); return "Iop_SarN32x2";
case Iop_QShl8x8: fprintf(stderr, "unimplemented Iop_QShl8x8 (%x)\n", op); return "Iop_QShl8x8";
case Iop_QShl16x4: fprintf(stderr, "unimplemented Iop_QShl16x4 (%x)\n", op); return "Iop_QShl16x4";
case Iop_QShl32x2: fprintf(stderr, "unimplemented Iop_QShl32x2 (%x)\n", op); return "Iop_QShl32x2";
case Iop_QShl64x1: fprintf(stderr, "unimplemented Iop_QShl64x1 (%x)\n", op); return "Iop_QShl64x1";
case Iop_QSal8x8: fprintf(stderr, "unimplemented Iop_QSal8x8 (%x)\n", op); return "Iop_QSal8x8";
case Iop_QSal16x4: fprintf(stderr, "unimplemented Iop_QSal16x4 (%x)\n", op); return "Iop_QSal16x4";
case Iop_QSal32x2: fprintf(stderr, "unimplemented Iop_QSal32x2 (%x)\n", op); return "Iop_QSal32x2";
case Iop_QSal64x1: fprintf(stderr, "unimplemented Iop_QSal64x1 (%x)\n", op); return "Iop_QSal64x1";
case Iop_QShlN8Sx8: fprintf(stderr, "unimplemented Iop_QShlN8Sx8 (%x)\n", op); return "Iop_QShlN8Sx8";
case Iop_QShlN16Sx4: fprintf(stderr, "unimplemented Iop_QShlN16Sx4 (%x)\n", op); return "Iop_QShlN16Sx4";
case Iop_QShlN32Sx2: fprintf(stderr, "unimplemented Iop_QShlN32Sx2 (%x)\n", op); return "Iop_QShlN32Sx2";
case Iop_QShlN64Sx1: fprintf(stderr, "unimplemented Iop_QShlN64Sx1 (%x)\n", op); return "Iop_QShlN64Sx1";
case Iop_QShlN8x8: fprintf(stderr, "unimplemented Iop_QShlN8x8 (%x)\n", op); return "Iop_QShlN8x8";
case Iop_QShlN16x4: fprintf(stderr, "unimplemented Iop_QShlN16x4 (%x)\n", op); return "Iop_QShlN16x4";
case Iop_QShlN32x2: fprintf(stderr, "unimplemented Iop_QShlN32x2 (%x)\n", op); return "Iop_QShlN32x2";
case Iop_QShlN64x1: fprintf(stderr, "unimplemented Iop_QShlN64x1 (%x)\n", op); return "Iop_QShlN64x1";
case Iop_QSalN8x8: fprintf(stderr, "unimplemented Iop_QSalN8x8 (%x)\n", op); return "Iop_QSalN8x8";
case Iop_QSalN16x4: fprintf(stderr, "unimplemented Iop_QSalN16x4 (%x)\n", op); return "Iop_QSalN16x4";
case Iop_QSalN32x2: fprintf(stderr, "unimplemented Iop_QSalN32x2 (%x)\n", op); return "Iop_QSalN32x2";
case Iop_QSalN64x1: fprintf(stderr, "unimplemented Iop_QSalN64x1 (%x)\n", op); return "Iop_QSalN64x1";
case Iop_QNarrow16Ux4: fprintf(stderr, "unimplemented Iop_QNarrow16Ux4 (%x)\n", op); return "Iop_QNarrow16Ux4";
case Iop_QNarrow16Sx4: fprintf(stderr, "unimplemented Iop_QNarrow16Sx4 (%x)\n", op); return "Iop_QNarrow16Sx4";
case Iop_QNarrow32Sx2: fprintf(stderr, "unimplemented Iop_QNarrow32Sx2 (%x)\n", op); return "Iop_QNarrow32Sx2";
case Iop_InterleaveHI8x8: fprintf(stderr, "unimplemented Iop_InterleaveHI8x8 (%x)\n", op); return "Iop_InterleaveHI8x8";
case Iop_InterleaveHI16x4: fprintf(stderr, "unimplemented Iop_InterleaveHI16x4 (%x)\n", op); return "Iop_InterleaveHI16x4";
case Iop_InterleaveHI32x2: fprintf(stderr, "unimplemented Iop_InterleaveHI32x2 (%x)\n", op); return "Iop_InterleaveHI32x2";
case Iop_InterleaveLO8x8: fprintf(stderr, "unimplemented Iop_InterleaveLO8x8 (%x)\n", op); return "Iop_InterleaveLO8x8";
case Iop_InterleaveLO16x4: fprintf(stderr, "unimplemented Iop_InterleaveLO16x4 (%x)\n", op); return "Iop_InterleaveLO16x4";
case Iop_InterleaveLO32x2: fprintf(stderr, "unimplemented Iop_InterleaveLO32x2 (%x)\n", op); return "Iop_InterleaveLO32x2";
case Iop_InterleaveOddLanes8x8: fprintf(stderr, "unimplemented Iop_InterleaveOddLanes8x8 (%x)\n", op); return "Iop_InterleaveOddLanes8x8";
case Iop_InterleaveEvenLanes8x8: fprintf(stderr, "unimplemented Iop_InterleaveEvenLanes8x8 (%x)\n", op); return "Iop_InterleaveEvenLanes8x8";
case Iop_InterleaveOddLanes16x4: fprintf(stderr, "unimplemented Iop_InterleaveOddLanes16x4 (%x)\n", op); return "Iop_InterleaveOddLanes16x4";
case Iop_InterleaveEvenLanes16x4: fprintf(stderr, "unimplemented Iop_InterleaveEvenLanes16x4 (%x)\n", op); return "Iop_InterleaveEvenLanes16x4";
case Iop_CatOddLanes8x8: fprintf(stderr, "unimplemented Iop_CatOddLanes8x8 (%x)\n", op); return "Iop_CatOddLanes8x8";
case Iop_CatOddLanes16x4: fprintf(stderr, "unimplemented Iop_CatOddLanes16x4 (%x)\n", op); return "Iop_CatOddLanes16x4";
case Iop_CatEvenLanes8x8: fprintf(stderr, "unimplemented Iop_CatEvenLanes8x8 (%x)\n", op); return "Iop_CatEvenLanes8x8";
case Iop_CatEvenLanes16x4: fprintf(stderr, "unimplemented Iop_CatEvenLanes16x4 (%x)\n", op); return "Iop_CatEvenLanes16x4";
case Iop_GetElem8x8: fprintf(stderr, "unimplemented Iop_GetElem8x8 (%x)\n", op); return "Iop_GetElem8x8";
case Iop_GetElem16x4: fprintf(stderr, "unimplemented Iop_GetElem16x4 (%x)\n", op); return "Iop_GetElem16x4";
case Iop_GetElem32x2: fprintf(stderr, "unimplemented Iop_GetElem32x2 (%x)\n", op); return "Iop_GetElem32x2";
case Iop_SetElem8x8: fprintf(stderr, "unimplemented Iop_SetElem8x8 (%x)\n", op); return "Iop_SetElem8x8";
case Iop_SetElem16x4: fprintf(stderr, "unimplemented Iop_SetElem16x4 (%x)\n", op); return "Iop_SetElem16x4";
case Iop_SetElem32x2: fprintf(stderr, "unimplemented Iop_SetElem32x2 (%x)\n", op); return "Iop_SetElem32x2";
case Iop_Dup8x8: fprintf(stderr, "unimplemented Iop_Dup8x8 (%x)\n", op); return "Iop_Dup8x8";
case Iop_Dup16x4: fprintf(stderr, "unimplemented Iop_Dup16x4 (%x)\n", op); return "Iop_Dup16x4";
case Iop_Dup32x2: fprintf(stderr, "unimplemented Iop_Dup32x2 (%x)\n", op); return "Iop_Dup32x2";
case Iop_Extract64: fprintf(stderr, "unimplemented Iop_Extract64 (%x)\n", op); return "Iop_Extract64";
case Iop_Reverse16_8x8: fprintf(stderr, "unimplemented Iop_Reverse16_8x8 (%x)\n", op); return "Iop_Reverse16_8x8";
case Iop_Reverse32_8x8: fprintf(stderr, "unimplemented Iop_Reverse32_8x8 (%x)\n", op); return "Iop_Reverse32_8x8";
case Iop_Reverse32_16x4: fprintf(stderr, "unimplemented Iop_Reverse32_16x4 (%x)\n", op); return "Iop_Reverse32_16x4";
case Iop_Reverse64_8x8: fprintf(stderr, "unimplemented Iop_Reverse64_8x8 (%x)\n", op); return "Iop_Reverse64_8x8";
case Iop_Reverse64_16x4: fprintf(stderr, "unimplemented Iop_Reverse64_16x4 (%x)\n", op); return "Iop_Reverse64_16x4";
case Iop_Reverse64_32x2: fprintf(stderr, "unimplemented Iop_Reverse64_32x2 (%x)\n", op); return "Iop_Reverse64_32x2";
case Iop_Perm8x8: fprintf(stderr, "unimplemented Iop_Perm8x8 (%x)\n", op); return "Iop_Perm8x8";
case Iop_Recip32x2: fprintf(stderr, "unimplemented Iop_Recip32x2 (%x)\n", op); return "Iop_Recip32x2";
case Iop_Rsqrte32x2: fprintf(stderr, "unimplemented Iop_Rsqrte32x2 (%x)\n", op); return "Iop_Rsqrte32x2";
case Iop_Add32Fx4: fprintf(stderr, "unimplemented Iop_Add32Fx4 (%x)\n", op); return "Iop_Add32Fx4";
case Iop_Sub32Fx4: fprintf(stderr, "unimplemented Iop_Sub32Fx4 (%x)\n", op); return "Iop_Sub32Fx4";
case Iop_Mul32Fx4: fprintf(stderr, "unimplemented Iop_Mul32Fx4 (%x)\n", op); return "Iop_Mul32Fx4";
case Iop_Div32Fx4: fprintf(stderr, "unimplemented Iop_Div32Fx4 (%x)\n", op); return "Iop_Div32Fx4";
case Iop_Max32Fx4: fprintf(stderr, "unimplemented Iop_Max32Fx4 (%x)\n", op); return "Iop_Max32Fx4";
case Iop_Min32Fx4: fprintf(stderr, "unimplemented Iop_Min32Fx4 (%x)\n", op); return "Iop_Min32Fx4";
case Iop_Add32Fx2: fprintf(stderr, "unimplemented Iop_Add32Fx2 (%x)\n", op); return "Iop_Add32Fx2";
case Iop_Sub32Fx2: fprintf(stderr, "unimplemented Iop_Sub32Fx2 (%x)\n", op); return "Iop_Sub32Fx2";
case Iop_CmpEQ32Fx4: fprintf(stderr, "unimplemented Iop_CmpEQ32Fx4 (%x)\n", op); return "Iop_CmpEQ32Fx4";
case Iop_CmpLT32Fx4: fprintf(stderr, "unimplemented Iop_CmpLT32Fx4 (%x)\n", op); return "Iop_CmpLT32Fx4";
case Iop_CmpLE32Fx4: fprintf(stderr, "unimplemented Iop_CmpLE32Fx4 (%x)\n", op); return "Iop_CmpLE32Fx4";
case Iop_CmpUN32Fx4: fprintf(stderr, "unimplemented Iop_CmpUN32Fx4 (%x)\n", op); return "Iop_CmpUN32Fx4";
case Iop_CmpGT32Fx4: fprintf(stderr, "unimplemented Iop_CmpGT32Fx4 (%x)\n", op); return "Iop_CmpGT32Fx4";
case Iop_CmpGE32Fx4: fprintf(stderr, "unimplemented Iop_CmpGE32Fx4 (%x)\n", op); return "Iop_CmpGE32Fx4";
case Iop_Abs32Fx4: fprintf(stderr, "unimplemented Iop_Abs32Fx4 (%x)\n", op); return "Iop_Abs32Fx4";
case Iop_PwMax32Fx4: fprintf(stderr, "unimplemented Iop_PwMax32Fx4 (%x)\n", op); return "Iop_PwMax32Fx4";
case Iop_PwMin32Fx4: fprintf(stderr, "unimplemented Iop_PwMin32Fx4 (%x)\n", op); return "Iop_PwMin32Fx4";
case Iop_Sqrt32Fx4: fprintf(stderr, "unimplemented Iop_Sqrt32Fx4 (%x)\n", op); return "Iop_Sqrt32Fx4";
case Iop_RSqrt32Fx4: fprintf(stderr, "unimplemented Iop_RSqrt32Fx4 (%x)\n", op); return "Iop_RSqrt32Fx4";
case Iop_Neg32Fx4: fprintf(stderr, "unimplemented Iop_Neg32Fx4 (%x)\n", op); return "Iop_Neg32Fx4";
case Iop_Recip32Fx4: fprintf(stderr, "unimplemented Iop_Recip32Fx4 (%x)\n", op); return "Iop_Recip32Fx4";
case Iop_Recps32Fx4: fprintf(stderr, "unimplemented Iop_Recps32Fx4 (%x)\n", op); return "Iop_Recps32Fx4";
case Iop_Rsqrte32Fx4: fprintf(stderr, "unimplemented Iop_Rsqrte32Fx4 (%x)\n", op); return "Iop_Rsqrte32Fx4";
case Iop_Rsqrts32Fx4: fprintf(stderr, "unimplemented Iop_Rsqrts32Fx4 (%x)\n", op); return "Iop_Rsqrts32Fx4";
case Iop_I32UtoFx4: fprintf(stderr, "unimplemented Iop_I32UtoFx4 (%x)\n", op); return "Iop_I32UtoFx4";
case Iop_I32StoFx4: fprintf(stderr, "unimplemented Iop_I32StoFx4 (%x)\n", op); return "Iop_I32StoFx4";
case Iop_FtoI32Ux4_RZ: fprintf(stderr, "unimplemented Iop_FtoI32Ux4_RZ (%x)\n", op); return "Iop_FtoI32Ux4_RZ";
case Iop_FtoI32Sx4_RZ: fprintf(stderr, "unimplemented Iop_FtoI32Sx4_RZ (%x)\n", op); return "Iop_FtoI32Sx4_RZ";
case Iop_QFtoI32Ux4_RZ: fprintf(stderr, "unimplemented Iop_QFtoI32Ux4_RZ (%x)\n", op); return "Iop_QFtoI32Ux4_RZ";
case Iop_QFtoI32Sx4_RZ: fprintf(stderr, "unimplemented Iop_QFtoI32Sx4_RZ (%x)\n", op); return "Iop_QFtoI32Sx4_RZ";
case Iop_RoundF32x4_RM: fprintf(stderr, "unimplemented Iop_RoundF32x4_RM (%x)\n", op); return "Iop_RoundF32x4_RM";
case Iop_RoundF32x4_RP: fprintf(stderr, "unimplemented Iop_RoundF32x4_RP (%x)\n", op); return "Iop_RoundF32x4_RP";
case Iop_RoundF32x4_RN: fprintf(stderr, "unimplemented Iop_RoundF32x4_RN (%x)\n", op); return "Iop_RoundF32x4_RN";
case Iop_RoundF32x4_RZ: fprintf(stderr, "unimplemented Iop_RoundF32x4_RZ (%x)\n", op); return "Iop_RoundF32x4_RZ";
case Iop_F32ToFixed32Ux4_RZ: fprintf(stderr, "unimplemented Iop_F32ToFixed32Ux4_RZ (%x)\n", op); return "Iop_F32ToFixed32Ux4_RZ";
case Iop_F32ToFixed32Sx4_RZ: fprintf(stderr, "unimplemented Iop_F32ToFixed32Sx4_RZ (%x)\n", op); return "Iop_F32ToFixed32Sx4_RZ";
case Iop_Fixed32UToF32x4_RN: fprintf(stderr, "unimplemented Iop_Fixed32UToF32x4_RN (%x)\n", op); return "Iop_Fixed32UToF32x4_RN";
case Iop_Fixed32SToF32x4_RN: fprintf(stderr, "unimplemented Iop_Fixed32SToF32x4_RN (%x)\n", op); return "Iop_Fixed32SToF32x4_RN";
case Iop_F32toF16x4: fprintf(stderr, "unimplemented Iop_F32toF16x4 (%x)\n", op); return "Iop_F32toF16x4";
case Iop_F16toF32x4: fprintf(stderr, "unimplemented Iop_F16toF32x4 (%x)\n", op); return "Iop_F16toF32x4";
case Iop_Add32F0x4: fprintf(stderr, "unimplemented Iop_Add32F0x4 (%x)\n", op); return "Iop_Add32F0x4";
case Iop_Sub32F0x4: fprintf(stderr, "unimplemented Iop_Sub32F0x4 (%x)\n", op); return "Iop_Sub32F0x4";
case Iop_Mul32F0x4: fprintf(stderr, "unimplemented Iop_Mul32F0x4 (%x)\n", op); return "Iop_Mul32F0x4";
case Iop_Div32F0x4: fprintf(stderr, "unimplemented Iop_Div32F0x4 (%x)\n", op); return "Iop_Div32F0x4";
case Iop_Max32F0x4: fprintf(stderr, "unimplemented Iop_Max32F0x4 (%x)\n", op); return "Iop_Max32F0x4";
case Iop_Min32F0x4: fprintf(stderr, "unimplemented Iop_Min32F0x4 (%x)\n", op); return "Iop_Min32F0x4";
case Iop_CmpEQ32F0x4: fprintf(stderr, "unimplemented Iop_CmpEQ32F0x4 (%x)\n", op); return "Iop_CmpEQ32F0x4";
case Iop_CmpLT32F0x4: fprintf(stderr, "unimplemented Iop_CmpLT32F0x4 (%x)\n", op); return "Iop_CmpLT32F0x4";
case Iop_CmpLE32F0x4: fprintf(stderr, "unimplemented Iop_CmpLE32F0x4 (%x)\n", op); return "Iop_CmpLE32F0x4";
case Iop_CmpUN32F0x4: fprintf(stderr, "unimplemented Iop_CmpUN32F0x4 (%x)\n", op); return "Iop_CmpUN32F0x4";
case Iop_Recip32F0x4: fprintf(stderr, "unimplemented Iop_Recip32F0x4 (%x)\n", op); return "Iop_Recip32F0x4";
case Iop_Sqrt32F0x4: fprintf(stderr, "unimplemented Iop_Sqrt32F0x4 (%x)\n", op); return "Iop_Sqrt32F0x4";
case Iop_RSqrt32F0x4: fprintf(stderr, "unimplemented Iop_RSqrt32F0x4 (%x)\n", op); return "Iop_RSqrt32F0x4";
case Iop_V128HIto64: fprintf(stderr, "unimplemented Iop_V128HIto64 (%x)\n", op); return "Iop_V128HIto64";
case Iop_64UtoV128: fprintf(stderr, "unimplemented Iop_64UtoV128 (%x)\n", op); return "Iop_64UtoV128";
case Iop_SetV128lo64: fprintf(stderr, "unimplemented Iop_SetV128lo64 (%x)\n", op); return "Iop_SetV128lo64";
case Iop_V128to32: fprintf(stderr, "unimplemented Iop_V128to32 (%x)\n", op); return "Iop_V128to32";
case Iop_SetV128lo32: fprintf(stderr, "unimplemented Iop_SetV128lo32 (%x)\n", op); return "Iop_SetV128lo32";
case Iop_NotV128: fprintf(stderr, "unimplemented Iop_NotV128 (%x)\n", op); return "Iop_NotV128";
case Iop_AndV128: fprintf(stderr, "unimplemented Iop_AndV128 (%x)\n", op); return "Iop_AndV128";
case Iop_OrV128: fprintf(stderr, "unimplemented Iop_OrV128 (%x)\n", op); return "Iop_OrV128";
case Iop_XorV128: fprintf(stderr, "unimplemented Iop_XorV128 (%x)\n", op); return "Iop_XorV128";
case Iop_ShlV128: fprintf(stderr, "unimplemented Iop_ShlV128 (%x)\n", op); return "Iop_ShlV128";
case Iop_ShrV128: fprintf(stderr, "unimplemented Iop_ShrV128 (%x)\n", op); return "Iop_ShrV128";
case Iop_CmpNEZ8x16: fprintf(stderr, "unimplemented Iop_CmpNEZ8x16 (%x)\n", op); return "Iop_CmpNEZ8x16";
case Iop_CmpNEZ16x8: fprintf(stderr, "unimplemented Iop_CmpNEZ16x8 (%x)\n", op); return "Iop_CmpNEZ16x8";
case Iop_CmpNEZ32x4: fprintf(stderr, "unimplemented Iop_CmpNEZ32x4 (%x)\n", op); return "Iop_CmpNEZ32x4";
case Iop_CmpNEZ64x2: fprintf(stderr, "unimplemented Iop_CmpNEZ64x2 (%x)\n", op); return "Iop_CmpNEZ64x2";
case Iop_Add8x16: fprintf(stderr, "unimplemented Iop_Add8x16 (%x)\n", op); return "Iop_Add8x16";
case Iop_Add16x8: fprintf(stderr, "unimplemented Iop_Add16x8 (%x)\n", op); return "Iop_Add16x8";
case Iop_Add32x4: fprintf(stderr, "unimplemented Iop_Add32x4 (%x)\n", op); return "Iop_Add32x4";
case Iop_Add64x2: fprintf(stderr, "unimplemented Iop_Add64x2 (%x)\n", op); return "Iop_Add64x2";
case Iop_QAdd8Ux16: fprintf(stderr, "unimplemented Iop_QAdd8Ux16 (%x)\n", op); return "Iop_QAdd8Ux16";
case Iop_QAdd16Ux8: fprintf(stderr, "unimplemented Iop_QAdd16Ux8 (%x)\n", op); return "Iop_QAdd16Ux8";
case Iop_QAdd32Ux4: fprintf(stderr, "unimplemented Iop_QAdd32Ux4 (%x)\n", op); return "Iop_QAdd32Ux4";
case Iop_QAdd64Ux2: fprintf(stderr, "unimplemented Iop_QAdd64Ux2 (%x)\n", op); return "Iop_QAdd64Ux2";
case Iop_QAdd8Sx16: fprintf(stderr, "unimplemented Iop_QAdd8Sx16 (%x)\n", op); return "Iop_QAdd8Sx16";
case Iop_QAdd16Sx8: fprintf(stderr, "unimplemented Iop_QAdd16Sx8 (%x)\n", op); return "Iop_QAdd16Sx8";
case Iop_QAdd32Sx4: fprintf(stderr, "unimplemented Iop_QAdd32Sx4 (%x)\n", op); return "Iop_QAdd32Sx4";
case Iop_QAdd64Sx2: fprintf(stderr, "unimplemented Iop_QAdd64Sx2 (%x)\n", op); return "Iop_QAdd64Sx2";
case Iop_Sub8x16: fprintf(stderr, "unimplemented Iop_Sub8x16 (%x)\n", op); return "Iop_Sub8x16";
case Iop_Sub16x8: fprintf(stderr, "unimplemented Iop_Sub16x8 (%x)\n", op); return "Iop_Sub16x8";
case Iop_Sub32x4: fprintf(stderr, "unimplemented Iop_Sub32x4 (%x)\n", op); return "Iop_Sub32x4";
case Iop_Sub64x2: fprintf(stderr, "unimplemented Iop_Sub64x2 (%x)\n", op); return "Iop_Sub64x2";
case Iop_QSub8Ux16: fprintf(stderr, "unimplemented Iop_QSub8Ux16 (%x)\n", op); return "Iop_QSub8Ux16";
case Iop_QSub16Ux8: fprintf(stderr, "unimplemented Iop_QSub16Ux8 (%x)\n", op); return "Iop_QSub16Ux8";
case Iop_QSub32Ux4: fprintf(stderr, "unimplemented Iop_QSub32Ux4 (%x)\n", op); return "Iop_QSub32Ux4";
case Iop_QSub64Ux2: fprintf(stderr, "unimplemented Iop_QSub64Ux2 (%x)\n", op); return "Iop_QSub64Ux2";
case Iop_QSub8Sx16: fprintf(stderr, "unimplemented Iop_QSub8Sx16 (%x)\n", op); return "Iop_QSub8Sx16";
case Iop_QSub16Sx8: fprintf(stderr, "unimplemented Iop_QSub16Sx8 (%x)\n", op); return "Iop_QSub16Sx8";
case Iop_QSub32Sx4: fprintf(stderr, "unimplemented Iop_QSub32Sx4 (%x)\n", op); return "Iop_QSub32Sx4";
case Iop_QSub64Sx2: fprintf(stderr, "unimplemented Iop_QSub64Sx2 (%x)\n", op); return "Iop_QSub64Sx2";
case Iop_Mul8x16: fprintf(stderr, "unimplemented Iop_Mul8x16 (%x)\n", op); return "Iop_Mul8x16";
case Iop_Mul16x8: fprintf(stderr, "unimplemented Iop_Mul16x8 (%x)\n", op); return "Iop_Mul16x8";
case Iop_Mul32x4: fprintf(stderr, "unimplemented Iop_Mul32x4 (%x)\n", op); return "Iop_Mul32x4";
case Iop_MulHi16Ux8: fprintf(stderr, "unimplemented Iop_MulHi16Ux8 (%x)\n", op); return "Iop_MulHi16Ux8";
case Iop_MulHi32Ux4: fprintf(stderr, "unimplemented Iop_MulHi32Ux4 (%x)\n", op); return "Iop_MulHi32Ux4";
case Iop_MulHi16Sx8: fprintf(stderr, "unimplemented Iop_MulHi16Sx8 (%x)\n", op); return "Iop_MulHi16Sx8";
case Iop_MulHi32Sx4: fprintf(stderr, "unimplemented Iop_MulHi32Sx4 (%x)\n", op); return "Iop_MulHi32Sx4";
case Iop_MullEven8Ux16: fprintf(stderr, "unimplemented Iop_MullEven8Ux16 (%x)\n", op); return "Iop_MullEven8Ux16";
case Iop_MullEven16Ux8: fprintf(stderr, "unimplemented Iop_MullEven16Ux8 (%x)\n", op); return "Iop_MullEven16Ux8";
case Iop_MullEven8Sx16: fprintf(stderr, "unimplemented Iop_MullEven8Sx16 (%x)\n", op); return "Iop_MullEven8Sx16";
case Iop_MullEven16Sx8: fprintf(stderr, "unimplemented Iop_MullEven16Sx8 (%x)\n", op); return "Iop_MullEven16Sx8";
case Iop_Mull8Ux8: fprintf(stderr, "unimplemented Iop_Mull8Ux8 (%x)\n", op); return "Iop_Mull8Ux8";
case Iop_Mull8Sx8: fprintf(stderr, "unimplemented Iop_Mull8Sx8 (%x)\n", op); return "Iop_Mull8Sx8";
case Iop_Mull16Ux4: fprintf(stderr, "unimplemented Iop_Mull16Ux4 (%x)\n", op); return "Iop_Mull16Ux4";
case Iop_Mull16Sx4: fprintf(stderr, "unimplemented Iop_Mull16Sx4 (%x)\n", op); return "Iop_Mull16Sx4";
case Iop_Mull32Ux2: fprintf(stderr, "unimplemented Iop_Mull32Ux2 (%x)\n", op); return "Iop_Mull32Ux2";
case Iop_Mull32Sx2: fprintf(stderr, "unimplemented Iop_Mull32Sx2 (%x)\n", op); return "Iop_Mull32Sx2";
case Iop_QDMulHi16Sx8: fprintf(stderr, "unimplemented Iop_QDMulHi16Sx8 (%x)\n", op); return "Iop_QDMulHi16Sx8";
case Iop_QDMulHi32Sx4: fprintf(stderr, "unimplemented Iop_QDMulHi32Sx4 (%x)\n", op); return "Iop_QDMulHi32Sx4";
case Iop_QRDMulHi16Sx8: fprintf(stderr, "unimplemented Iop_QRDMulHi16Sx8 (%x)\n", op); return "Iop_QRDMulHi16Sx8";
case Iop_QRDMulHi32Sx4: fprintf(stderr, "unimplemented Iop_QRDMulHi32Sx4 (%x)\n", op); return "Iop_QRDMulHi32Sx4";
case Iop_QDMulLong16Sx4: fprintf(stderr, "unimplemented Iop_QDMulLong16Sx4 (%x)\n", op); return "Iop_QDMulLong16Sx4";
case Iop_QDMulLong32Sx2: fprintf(stderr, "unimplemented Iop_QDMulLong32Sx2 (%x)\n", op); return "Iop_QDMulLong32Sx2";
case Iop_PolynomialMul8x16: fprintf(stderr, "unimplemented Iop_PolynomialMul8x16 (%x)\n", op); return "Iop_PolynomialMul8x16";
case Iop_PolynomialMull8x8: fprintf(stderr, "unimplemented Iop_PolynomialMull8x8 (%x)\n", op); return "Iop_PolynomialMull8x8";
case Iop_PwAdd8x16: fprintf(stderr, "unimplemented Iop_PwAdd8x16 (%x)\n", op); return "Iop_PwAdd8x16";
case Iop_PwAdd16x8: fprintf(stderr, "unimplemented Iop_PwAdd16x8 (%x)\n", op); return "Iop_PwAdd16x8";
case Iop_PwAdd32x4: fprintf(stderr, "unimplemented Iop_PwAdd32x4 (%x)\n", op); return "Iop_PwAdd32x4";
case Iop_PwAdd32Fx2: fprintf(stderr, "unimplemented Iop_PwAdd32Fx2 (%x)\n", op); return "Iop_PwAdd32Fx2";
case Iop_PwAddL8Ux16: fprintf(stderr, "unimplemented Iop_PwAddL8Ux16 (%x)\n", op); return "Iop_PwAddL8Ux16";
case Iop_PwAddL16Ux8: fprintf(stderr, "unimplemented Iop_PwAddL16Ux8 (%x)\n", op); return "Iop_PwAddL16Ux8";
case Iop_PwAddL32Ux4: fprintf(stderr, "unimplemented Iop_PwAddL32Ux4 (%x)\n", op); return "Iop_PwAddL32Ux4";
case Iop_PwAddL8Sx16: fprintf(stderr, "unimplemented Iop_PwAddL8Sx16 (%x)\n", op); return "Iop_PwAddL8Sx16";
case Iop_PwAddL16Sx8: fprintf(stderr, "unimplemented Iop_PwAddL16Sx8 (%x)\n", op); return "Iop_PwAddL16Sx8";
case Iop_PwAddL32Sx4: fprintf(stderr, "unimplemented Iop_PwAddL32Sx4 (%x)\n", op); return "Iop_PwAddL32Sx4";
case Iop_Abs8x16: fprintf(stderr, "unimplemented Iop_Abs8x16 (%x)\n", op); return "Iop_Abs8x16";
case Iop_Abs16x8: fprintf(stderr, "unimplemented Iop_Abs16x8 (%x)\n", op); return "Iop_Abs16x8";
case Iop_Abs32x4: fprintf(stderr, "unimplemented Iop_Abs32x4 (%x)\n", op); return "Iop_Abs32x4";
case Iop_Avg8Ux16: fprintf(stderr, "unimplemented Iop_Avg8Ux16 (%x)\n", op); return "Iop_Avg8Ux16";
case Iop_Avg16Ux8: fprintf(stderr, "unimplemented Iop_Avg16Ux8 (%x)\n", op); return "Iop_Avg16Ux8";
case Iop_Avg32Ux4: fprintf(stderr, "unimplemented Iop_Avg32Ux4 (%x)\n", op); return "Iop_Avg32Ux4";
case Iop_Avg8Sx16: fprintf(stderr, "unimplemented Iop_Avg8Sx16 (%x)\n", op); return "Iop_Avg8Sx16";
case Iop_Avg16Sx8: fprintf(stderr, "unimplemented Iop_Avg16Sx8 (%x)\n", op); return "Iop_Avg16Sx8";
case Iop_Avg32Sx4: fprintf(stderr, "unimplemented Iop_Avg32Sx4 (%x)\n", op); return "Iop_Avg32Sx4";
case Iop_Max8Sx16: fprintf(stderr, "unimplemented Iop_Max8Sx16 (%x)\n", op); return "Iop_Max8Sx16";
case Iop_Max16Sx8: fprintf(stderr, "unimplemented Iop_Max16Sx8 (%x)\n", op); return "Iop_Max16Sx8";
case Iop_Max32Sx4: fprintf(stderr, "unimplemented Iop_Max32Sx4 (%x)\n", op); return "Iop_Max32Sx4";
case Iop_Max8Ux16: fprintf(stderr, "unimplemented Iop_Max8Ux16 (%x)\n", op); return "Iop_Max8Ux16";
case Iop_Max16Ux8: fprintf(stderr, "unimplemented Iop_Max16Ux8 (%x)\n", op); return "Iop_Max16Ux8";
case Iop_Max32Ux4: fprintf(stderr, "unimplemented Iop_Max32Ux4 (%x)\n", op); return "Iop_Max32Ux4";
case Iop_Min8Sx16: fprintf(stderr, "unimplemented Iop_Min8Sx16 (%x)\n", op); return "Iop_Min8Sx16";
case Iop_Min16Sx8: fprintf(stderr, "unimplemented Iop_Min16Sx8 (%x)\n", op); return "Iop_Min16Sx8";
case Iop_Min32Sx4: fprintf(stderr, "unimplemented Iop_Min32Sx4 (%x)\n", op); return "Iop_Min32Sx4";
case Iop_Min8Ux16: fprintf(stderr, "unimplemented Iop_Min8Ux16 (%x)\n", op); return "Iop_Min8Ux16";
case Iop_Min16Ux8: fprintf(stderr, "unimplemented Iop_Min16Ux8 (%x)\n", op); return "Iop_Min16Ux8";
case Iop_Min32Ux4: fprintf(stderr, "unimplemented Iop_Min32Ux4 (%x)\n", op); return "Iop_Min32Ux4";
case Iop_CmpEQ8x16: fprintf(stderr, "unimplemented Iop_CmpEQ8x16 (%x)\n", op); return "Iop_CmpEQ8x16";
case Iop_CmpEQ16x8: fprintf(stderr, "unimplemented Iop_CmpEQ16x8 (%x)\n", op); return "Iop_CmpEQ16x8";
case Iop_CmpEQ32x4: fprintf(stderr, "unimplemented Iop_CmpEQ32x4 (%x)\n", op); return "Iop_CmpEQ32x4";
case Iop_CmpGT8Sx16: fprintf(stderr, "unimplemented Iop_CmpGT8Sx16 (%x)\n", op); return "Iop_CmpGT8Sx16";
case Iop_CmpGT16Sx8: fprintf(stderr, "unimplemented Iop_CmpGT16Sx8 (%x)\n", op); return "Iop_CmpGT16Sx8";
case Iop_CmpGT32Sx4: fprintf(stderr, "unimplemented Iop_CmpGT32Sx4 (%x)\n", op); return "Iop_CmpGT32Sx4";
case Iop_CmpGT64Sx2: fprintf(stderr, "unimplemented Iop_CmpGT64Sx2 (%x)\n", op); return "Iop_CmpGT64Sx2";
case Iop_CmpGT8Ux16: fprintf(stderr, "unimplemented Iop_CmpGT8Ux16 (%x)\n", op); return "Iop_CmpGT8Ux16";
case Iop_CmpGT16Ux8: fprintf(stderr, "unimplemented Iop_CmpGT16Ux8 (%x)\n", op); return "Iop_CmpGT16Ux8";
case Iop_CmpGT32Ux4: fprintf(stderr, "unimplemented Iop_CmpGT32Ux4 (%x)\n", op); return "Iop_CmpGT32Ux4";
case Iop_Cnt8x16: fprintf(stderr, "unimplemented Iop_Cnt8x16 (%x)\n", op); return "Iop_Cnt8x16";
case Iop_Clz8Sx16: fprintf(stderr, "unimplemented Iop_Clz8Sx16 (%x)\n", op); return "Iop_Clz8Sx16";
case Iop_Clz16Sx8: fprintf(stderr, "unimplemented Iop_Clz16Sx8 (%x)\n", op); return "Iop_Clz16Sx8";
case Iop_Clz32Sx4: fprintf(stderr, "unimplemented Iop_Clz32Sx4 (%x)\n", op); return "Iop_Clz32Sx4";
case Iop_Cls8Sx16: fprintf(stderr, "unimplemented Iop_Cls8Sx16 (%x)\n", op); return "Iop_Cls8Sx16";
case Iop_Cls16Sx8: fprintf(stderr, "unimplemented Iop_Cls16Sx8 (%x)\n", op); return "Iop_Cls16Sx8";
case Iop_Cls32Sx4: fprintf(stderr, "unimplemented Iop_Cls32Sx4 (%x)\n", op); return "Iop_Cls32Sx4";
case Iop_ShlN8x16: fprintf(stderr, "unimplemented Iop_ShlN8x16 (%x)\n", op); return "Iop_ShlN8x16";
case Iop_ShlN16x8: fprintf(stderr, "unimplemented Iop_ShlN16x8 (%x)\n", op); return "Iop_ShlN16x8";
case Iop_ShlN32x4: fprintf(stderr, "unimplemented Iop_ShlN32x4 (%x)\n", op); return "Iop_ShlN32x4";
case Iop_ShlN64x2: fprintf(stderr, "unimplemented Iop_ShlN64x2 (%x)\n", op); return "Iop_ShlN64x2";
case Iop_ShrN8x16: fprintf(stderr, "unimplemented Iop_ShrN8x16 (%x)\n", op); return "Iop_ShrN8x16";
case Iop_ShrN16x8: fprintf(stderr, "unimplemented Iop_ShrN16x8 (%x)\n", op); return "Iop_ShrN16x8";
case Iop_ShrN32x4: fprintf(stderr, "unimplemented Iop_ShrN32x4 (%x)\n", op); return "Iop_ShrN32x4";
case Iop_ShrN64x2: fprintf(stderr, "unimplemented Iop_ShrN64x2 (%x)\n", op); return "Iop_ShrN64x2";
case Iop_SarN8x16: fprintf(stderr, "unimplemented Iop_SarN8x16 (%x)\n", op); return "Iop_SarN8x16";
case Iop_SarN16x8: fprintf(stderr, "unimplemented Iop_SarN16x8 (%x)\n", op); return "Iop_SarN16x8";
case Iop_SarN32x4: fprintf(stderr, "unimplemented Iop_SarN32x4 (%x)\n", op); return "Iop_SarN32x4";
case Iop_SarN64x2: fprintf(stderr, "unimplemented Iop_SarN64x2 (%x)\n", op); return "Iop_SarN64x2";
case Iop_Shl8x16: fprintf(stderr, "unimplemented Iop_Shl8x16 (%x)\n", op); return "Iop_Shl8x16";
case Iop_Shl16x8: fprintf(stderr, "unimplemented Iop_Shl16x8 (%x)\n", op); return "Iop_Shl16x8";
case Iop_Shl32x4: fprintf(stderr, "unimplemented Iop_Shl32x4 (%x)\n", op); return "Iop_Shl32x4";
case Iop_Shl64x2: fprintf(stderr, "unimplemented Iop_Shl64x2 (%x)\n", op); return "Iop_Shl64x2";
case Iop_Shr8x16: fprintf(stderr, "unimplemented Iop_Shr8x16 (%x)\n", op); return "Iop_Shr8x16";
case Iop_Shr16x8: fprintf(stderr, "unimplemented Iop_Shr16x8 (%x)\n", op); return "Iop_Shr16x8";
case Iop_Shr32x4: fprintf(stderr, "unimplemented Iop_Shr32x4 (%x)\n", op); return "Iop_Shr32x4";
case Iop_Shr64x2: fprintf(stderr, "unimplemented Iop_Shr64x2 (%x)\n", op); return "Iop_Shr64x2";
case Iop_Sar8x16: fprintf(stderr, "unimplemented Iop_Sar8x16 (%x)\n", op); return "Iop_Sar8x16";
case Iop_Sar16x8: fprintf(stderr, "unimplemented Iop_Sar16x8 (%x)\n", op); return "Iop_Sar16x8";
case Iop_Sar32x4: fprintf(stderr, "unimplemented Iop_Sar32x4 (%x)\n", op); return "Iop_Sar32x4";
case Iop_Sar64x2: fprintf(stderr, "unimplemented Iop_Sar64x2 (%x)\n", op); return "Iop_Sar64x2";
case Iop_Sal8x16: fprintf(stderr, "unimplemented Iop_Sal8x16 (%x)\n", op); return "Iop_Sal8x16";
case Iop_Sal16x8: fprintf(stderr, "unimplemented Iop_Sal16x8 (%x)\n", op); return "Iop_Sal16x8";
case Iop_Sal32x4: fprintf(stderr, "unimplemented Iop_Sal32x4 (%x)\n", op); return "Iop_Sal32x4";
case Iop_Sal64x2: fprintf(stderr, "unimplemented Iop_Sal64x2 (%x)\n", op); return "Iop_Sal64x2";
case Iop_Rol8x16: fprintf(stderr, "unimplemented Iop_Rol8x16 (%x)\n", op); return "Iop_Rol8x16";
case Iop_Rol16x8: fprintf(stderr, "unimplemented Iop_Rol16x8 (%x)\n", op); return "Iop_Rol16x8";
case Iop_Rol32x4: fprintf(stderr, "unimplemented Iop_Rol32x4 (%x)\n", op); return "Iop_Rol32x4";
case Iop_QShl8x16: fprintf(stderr, "unimplemented Iop_QShl8x16 (%x)\n", op); return "Iop_QShl8x16";
case Iop_QShl16x8: fprintf(stderr, "unimplemented Iop_QShl16x8 (%x)\n", op); return "Iop_QShl16x8";
case Iop_QShl32x4: fprintf(stderr, "unimplemented Iop_QShl32x4 (%x)\n", op); return "Iop_QShl32x4";
case Iop_QShl64x2: fprintf(stderr, "unimplemented Iop_QShl64x2 (%x)\n", op); return "Iop_QShl64x2";
case Iop_QSal8x16: fprintf(stderr, "unimplemented Iop_QSal8x16 (%x)\n", op); return "Iop_QSal8x16";
case Iop_QSal16x8: fprintf(stderr, "unimplemented Iop_QSal16x8 (%x)\n", op); return "Iop_QSal16x8";
case Iop_QSal32x4: fprintf(stderr, "unimplemented Iop_QSal32x4 (%x)\n", op); return "Iop_QSal32x4";
case Iop_QSal64x2: fprintf(stderr, "unimplemented Iop_QSal64x2 (%x)\n", op); return "Iop_QSal64x2";
case Iop_QShlN8Sx16: fprintf(stderr, "unimplemented Iop_QShlN8Sx16 (%x)\n", op); return "Iop_QShlN8Sx16";
case Iop_QShlN16Sx8: fprintf(stderr, "unimplemented Iop_QShlN16Sx8 (%x)\n", op); return "Iop_QShlN16Sx8";
case Iop_QShlN32Sx4: fprintf(stderr, "unimplemented Iop_QShlN32Sx4 (%x)\n", op); return "Iop_QShlN32Sx4";
case Iop_QShlN64Sx2: fprintf(stderr, "unimplemented Iop_QShlN64Sx2 (%x)\n", op); return "Iop_QShlN64Sx2";
case Iop_QShlN8x16: fprintf(stderr, "unimplemented Iop_QShlN8x16 (%x)\n", op); return "Iop_QShlN8x16";
case Iop_QShlN16x8: fprintf(stderr, "unimplemented Iop_QShlN16x8 (%x)\n", op); return "Iop_QShlN16x8";
case Iop_QShlN32x4: fprintf(stderr, "unimplemented Iop_QShlN32x4 (%x)\n", op); return "Iop_QShlN32x4";
case Iop_QShlN64x2: fprintf(stderr, "unimplemented Iop_QShlN64x2 (%x)\n", op); return "Iop_QShlN64x2";
case Iop_QSalN8x16: fprintf(stderr, "unimplemented Iop_QSalN8x16 (%x)\n", op); return "Iop_QSalN8x16";
case Iop_QSalN16x8: fprintf(stderr, "unimplemented Iop_QSalN16x8 (%x)\n", op); return "Iop_QSalN16x8";
case Iop_QSalN32x4: fprintf(stderr, "unimplemented Iop_QSalN32x4 (%x)\n", op); return "Iop_QSalN32x4";
case Iop_QSalN64x2: fprintf(stderr, "unimplemented Iop_QSalN64x2 (%x)\n", op); return "Iop_QSalN64x2";
case Iop_QNarrow16Ux8: fprintf(stderr, "unimplemented Iop_QNarrow16Ux8 (%x)\n", op); return "Iop_QNarrow16Ux8";
case Iop_QNarrow32Ux4: fprintf(stderr, "unimplemented Iop_QNarrow32Ux4 (%x)\n", op); return "Iop_QNarrow32Ux4";
case Iop_QNarrow16Sx8: fprintf(stderr, "unimplemented Iop_QNarrow16Sx8 (%x)\n", op); return "Iop_QNarrow16Sx8";
case Iop_QNarrow32Sx4: fprintf(stderr, "unimplemented Iop_QNarrow32Sx4 (%x)\n", op); return "Iop_QNarrow32Sx4";
case Iop_Narrow16x8: fprintf(stderr, "unimplemented Iop_Narrow16x8 (%x)\n", op); return "Iop_Narrow16x8";
case Iop_Narrow32x4: fprintf(stderr, "unimplemented Iop_Narrow32x4 (%x)\n", op); return "Iop_Narrow32x4";
case Iop_Shorten16x8: fprintf(stderr, "unimplemented Iop_Shorten16x8 (%x)\n", op); return "Iop_Shorten16x8";
case Iop_Shorten32x4: fprintf(stderr, "unimplemented Iop_Shorten32x4 (%x)\n", op); return "Iop_Shorten32x4";
case Iop_Shorten64x2: fprintf(stderr, "unimplemented Iop_Shorten64x2 (%x)\n", op); return "Iop_Shorten64x2";
case Iop_QShortenS16Sx8: fprintf(stderr, "unimplemented Iop_QShortenS16Sx8 (%x)\n", op); return "Iop_QShortenS16Sx8";
case Iop_QShortenS32Sx4: fprintf(stderr, "unimplemented Iop_QShortenS32Sx4 (%x)\n", op); return "Iop_QShortenS32Sx4";
case Iop_QShortenS64Sx2: fprintf(stderr, "unimplemented Iop_QShortenS64Sx2 (%x)\n", op); return "Iop_QShortenS64Sx2";
case Iop_QShortenU16Sx8: fprintf(stderr, "unimplemented Iop_QShortenU16Sx8 (%x)\n", op); return "Iop_QShortenU16Sx8";
case Iop_QShortenU32Sx4: fprintf(stderr, "unimplemented Iop_QShortenU32Sx4 (%x)\n", op); return "Iop_QShortenU32Sx4";
case Iop_QShortenU64Sx2: fprintf(stderr, "unimplemented Iop_QShortenU64Sx2 (%x)\n", op); return "Iop_QShortenU64Sx2";
case Iop_QShortenU16Ux8: fprintf(stderr, "unimplemented Iop_QShortenU16Ux8 (%x)\n", op); return "Iop_QShortenU16Ux8";
case Iop_QShortenU32Ux4: fprintf(stderr, "unimplemented Iop_QShortenU32Ux4 (%x)\n", op); return "Iop_QShortenU32Ux4";
case Iop_QShortenU64Ux2: fprintf(stderr, "unimplemented Iop_QShortenU64Ux2 (%x)\n", op); return "Iop_QShortenU64Ux2";
case Iop_Longen8Ux8: fprintf(stderr, "unimplemented Iop_Longen8Ux8 (%x)\n", op); return "Iop_Longen8Ux8";
case Iop_Longen16Ux4: fprintf(stderr, "unimplemented Iop_Longen16Ux4 (%x)\n", op); return "Iop_Longen16Ux4";
case Iop_Longen32Ux2: fprintf(stderr, "unimplemented Iop_Longen32Ux2 (%x)\n", op); return "Iop_Longen32Ux2";
case Iop_Longen8Sx8: fprintf(stderr, "unimplemented Iop_Longen8Sx8 (%x)\n", op); return "Iop_Longen8Sx8";
case Iop_Longen16Sx4: fprintf(stderr, "unimplemented Iop_Longen16Sx4 (%x)\n", op); return "Iop_Longen16Sx4";
case Iop_Longen32Sx2: fprintf(stderr, "unimplemented Iop_Longen32Sx2 (%x)\n", op); return "Iop_Longen32Sx2";
case Iop_InterleaveHI8x16: fprintf(stderr, "unimplemented Iop_InterleaveHI8x16 (%x)\n", op); return "Iop_InterleaveHI8x16";
case Iop_InterleaveHI16x8: fprintf(stderr, "unimplemented Iop_InterleaveHI16x8 (%x)\n", op); return "Iop_InterleaveHI16x8";
case Iop_InterleaveHI32x4: fprintf(stderr, "unimplemented Iop_InterleaveHI32x4 (%x)\n", op); return "Iop_InterleaveHI32x4";
case Iop_InterleaveHI64x2: fprintf(stderr, "unimplemented Iop_InterleaveHI64x2 (%x)\n", op); return "Iop_InterleaveHI64x2";
case Iop_InterleaveLO16x8: fprintf(stderr, "unimplemented Iop_InterleaveLO16x8 (%x)\n", op); return "Iop_InterleaveLO16x8";
case Iop_InterleaveLO32x4: fprintf(stderr, "unimplemented Iop_InterleaveLO32x4 (%x)\n", op); return "Iop_InterleaveLO32x4";
case Iop_InterleaveLO64x2: fprintf(stderr, "unimplemented Iop_InterleaveLO64x2 (%x)\n", op); return "Iop_InterleaveLO64x2";
case Iop_InterleaveOddLanes8x16: fprintf(stderr, "unimplemented Iop_InterleaveOddLanes8x16 (%x)\n", op); return "Iop_InterleaveOddLanes8x16";
case Iop_InterleaveEvenLanes8x16: fprintf(stderr, "unimplemented Iop_InterleaveEvenLanes8x16 (%x)\n", op); return "Iop_InterleaveEvenLanes8x16";
case Iop_InterleaveOddLanes16x8: fprintf(stderr, "unimplemented Iop_InterleaveOddLanes16x8 (%x)\n", op); return "Iop_InterleaveOddLanes16x8";
case Iop_InterleaveEvenLanes16x8: fprintf(stderr, "unimplemented Iop_InterleaveEvenLanes16x8 (%x)\n", op); return "Iop_InterleaveEvenLanes16x8";
case Iop_InterleaveOddLanes32x4: fprintf(stderr, "unimplemented Iop_InterleaveOddLanes32x4 (%x)\n", op); return "Iop_InterleaveOddLanes32x4";
case Iop_InterleaveEvenLanes32x4: fprintf(stderr, "unimplemented Iop_InterleaveEvenLanes32x4 (%x)\n", op); return "Iop_InterleaveEvenLanes32x4";
case Iop_CatOddLanes8x16: fprintf(stderr, "unimplemented Iop_CatOddLanes8x16 (%x)\n", op); return "Iop_CatOddLanes8x16";
case Iop_CatOddLanes16x8: fprintf(stderr, "unimplemented Iop_CatOddLanes16x8 (%x)\n", op); return "Iop_CatOddLanes16x8";
case Iop_CatOddLanes32x4: fprintf(stderr, "unimplemented Iop_CatOddLanes32x4 (%x)\n", op); return "Iop_CatOddLanes32x4";
case Iop_CatEvenLanes8x16: fprintf(stderr, "unimplemented Iop_CatEvenLanes8x16 (%x)\n", op); return "Iop_CatEvenLanes8x16";
case Iop_CatEvenLanes16x8: fprintf(stderr, "unimplemented Iop_CatEvenLanes16x8 (%x)\n", op); return "Iop_CatEvenLanes16x8";
case Iop_CatEvenLanes32x4: fprintf(stderr, "unimplemented Iop_CatEvenLanes32x4 (%x)\n", op); return "Iop_CatEvenLanes32x4";
case Iop_GetElem8x16: fprintf(stderr, "unimplemented Iop_GetElem8x16 (%x)\n", op); return "Iop_GetElem8x16";
case Iop_GetElem16x8: fprintf(stderr, "unimplemented Iop_GetElem16x8 (%x)\n", op); return "Iop_GetElem16x8";
case Iop_GetElem32x4: fprintf(stderr, "unimplemented Iop_GetElem32x4 (%x)\n", op); return "Iop_GetElem32x4";
case Iop_GetElem64x2: fprintf(stderr, "unimplemented Iop_GetElem64x2 (%x)\n", op); return "Iop_GetElem64x2";
case Iop_Dup8x16: fprintf(stderr, "unimplemented Iop_Dup8x16 (%x)\n", op); return "Iop_Dup8x16";
case Iop_Dup16x8: fprintf(stderr, "unimplemented Iop_Dup16x8 (%x)\n", op); return "Iop_Dup16x8";
case Iop_Dup32x4: fprintf(stderr, "unimplemented Iop_Dup32x4 (%x)\n", op); return "Iop_Dup32x4";
case Iop_ExtractV128: fprintf(stderr, "unimplemented Iop_ExtractV128 (%x)\n", op); return "Iop_ExtractV128";
case Iop_Reverse16_8x16: fprintf(stderr, "unimplemented Iop_Reverse16_8x16 (%x)\n", op); return "Iop_Reverse16_8x16";
case Iop_Reverse32_8x16: fprintf(stderr, "unimplemented Iop_Reverse32_8x16 (%x)\n", op); return "Iop_Reverse32_8x16";
case Iop_Reverse32_16x8: fprintf(stderr, "unimplemented Iop_Reverse32_16x8 (%x)\n", op); return "Iop_Reverse32_16x8";
case Iop_Reverse64_8x16: fprintf(stderr, "unimplemented Iop_Reverse64_8x16 (%x)\n", op); return "Iop_Reverse64_8x16";
case Iop_Reverse64_16x8: fprintf(stderr, "unimplemented Iop_Reverse64_16x8 (%x)\n", op); return "Iop_Reverse64_16x8";
case Iop_Reverse64_32x4: fprintf(stderr, "unimplemented Iop_Reverse64_32x4 (%x)\n", op); return "Iop_Reverse64_32x4";
case Iop_Perm8x16: fprintf(stderr, "unimplemented Iop_Perm8x16 (%x)\n", op); return "Iop_Perm8x16";
case Iop_Recip32x4: fprintf(stderr, "unimplemented Iop_Recip32x4 (%x)\n", op); return "Iop_Recip32x4";
case Iop_Rsqrte32x4: fprintf(stderr, "unimplemented Iop_Rsqrte32x4 (%x)\n", op); return "Iop_Rsqrte32x4";



	case Iop_INVALID: return "Iop_INVALID";
	default:
	fprintf(stderr, "Unknown opcode %x, %x\n", op, Iop_InterleaveLO64x2);
	}
	return "???Op???";
}

#define UNOP_SETUP				\
	Value		*v1;			\
	IRBuilder<>	*builder;		\
	builder = theGenLLVM->getBuilder();	\
	v1 = args[0]->emit();

#define BINOP_SETUP				\
	Value		*v1, *v2;		\
	IRBuilder<>	*builder;		\
	v1 = args[0]->emit();			\
	v2 = args[1]->emit();			\
	builder = theGenLLVM->getBuilder();

#define UNOP_EMIT(x,y)				\
Value* VexExprUnop##x::emit(void) const	\
{						\
	UNOP_SETUP				\
	builder = theGenLLVM->getBuilder();	\
	return builder->y(v1);			\
}

#define X_TO_Y_EMIT(x,y,z)			\
Value* VexExprUnop##x::emit(void) const		\
{						\
	UNOP_SETUP				\
	return builder->y(v1, builder->z());	\
}

/* XXX probably wrong to discard rounding info, but who cares */
#define X_TO_Y_EMIT_ROUND(x,y,z)		\
Value* VexExprBinop##x::emit(void) const	\
{						\
	BINOP_SETUP				\
	return builder->y(v2, builder->z());	\
}

#define XHI_TO_Y_EMIT(x,y,z)			\
Value* VexExprUnop##x::emit(void) const		\
{						\
	UNOP_SETUP				\
	return builder->CreateTrunc(		\
		builder->CreateLShr(v1, y),	\
		builder->z());			\
}						\

XHI_TO_Y_EMIT(64HIto32, 32, getInt32Ty)

X_TO_Y_EMIT(64to32, CreateTrunc, getInt32Ty)
X_TO_Y_EMIT(32Uto64,CreateZExt, getInt64Ty)
X_TO_Y_EMIT(32Sto64, CreateSExt, getInt64Ty)
X_TO_Y_EMIT(32to8, CreateTrunc, getInt8Ty)
X_TO_Y_EMIT(32to16, CreateTrunc, getInt16Ty)
X_TO_Y_EMIT(64to1, CreateTrunc, getInt1Ty)
X_TO_Y_EMIT(64to8, CreateTrunc, getInt8Ty)
X_TO_Y_EMIT(64to16, CreateTrunc, getInt16Ty)
X_TO_Y_EMIT(1Uto8, CreateZExt, getInt8Ty)
X_TO_Y_EMIT(1Uto64, CreateZExt, getInt64Ty)
X_TO_Y_EMIT(16Uto64, CreateZExt, getInt64Ty)
X_TO_Y_EMIT(16Sto64, CreateSExt, getInt64Ty)
X_TO_Y_EMIT(16Uto32, CreateZExt, getInt32Ty)
X_TO_Y_EMIT(16Sto32, CreateSExt, getInt32Ty)
X_TO_Y_EMIT(8Uto16, CreateZExt, getInt16Ty)
X_TO_Y_EMIT(8Sto16, CreateSExt, getInt16Ty)
X_TO_Y_EMIT(8Uto32, CreateZExt, getInt32Ty)
X_TO_Y_EMIT(8Sto32, CreateSExt, getInt32Ty)
X_TO_Y_EMIT(8Uto64, CreateZExt, getInt64Ty)
X_TO_Y_EMIT(8Sto64, CreateSExt, getInt64Ty)
X_TO_Y_EMIT(128to64, CreateTrunc, getInt64Ty)
X_TO_Y_EMIT(F32toF64, CreateFPExt, getDoubleTy)

X_TO_Y_EMIT(I32StoF64, CreateSIToFP, getDoubleTy)

X_TO_Y_EMIT_ROUND(F64toF32, CreateFPTrunc, getFloatTy)
X_TO_Y_EMIT_ROUND(I64StoF64, CreateSIToFP, getDoubleTy)
X_TO_Y_EMIT_ROUND(I64UtoF64, CreateSIToFP, getDoubleTy)
X_TO_Y_EMIT_ROUND(F64toI32S, CreateFPToSI, getInt32Ty)
X_TO_Y_EMIT_ROUND(F64toI32U, CreateFPToSI, getInt32Ty)
X_TO_Y_EMIT_ROUND(F64toI64S, CreateFPToSI, getInt64Ty)

UNOP_EMIT(Not1, CreateNot)
UNOP_EMIT(Not8, CreateNot)
UNOP_EMIT(Not16, CreateNot)
UNOP_EMIT(Not32, CreateNot)
UNOP_EMIT(Not64, CreateNot)
UNOP_EMIT(NotV128, CreateNot)

#define get_vt_8x16() VectorType::get(Type::getInt16Ty(getGlobalContext()), 8)
#define get_vt_8x8() VectorType::get(Type::getInt8Ty(getGlobalContext()), 8)
#define get_vt_4x16() VectorType::get(Type::getInt16Ty(getGlobalContext()), 4)
#define get_vt_16x8() VectorType::get(Type::getInt8Ty(getGlobalContext()), 16)
#define get_vt_4xf32() VectorType::get(Type::getFloatTy(getGlobalContext()), 4)
#define get_vt_4x32() VectorType::get(Type::getInt32Ty(getGlobalContext()), 4)
#define get_vt_2xf64() VectorType::get(Type::getDoubleTy(getGlobalContext()), 2)


#define get_i32(x) ConstantInt::get(getGlobalContext(), APInt(32, x))

Value* VexExprUnopV128to64::emit(void) const
{
	Value		*v_hilo;
	Constant	*shuffle_v[] = {
		get_i32(0), get_i32(1),
		get_i32(2), get_i32(3),
		get_i32(4), get_i32(5),
		get_i32(6), get_i32(7),
	};
	Constant	*cv;

	UNOP_SETUP
	cv = ConstantVector::get(
		std::vector<Constant*>(
			shuffle_v,
			shuffle_v + sizeof(shuffle_v)/sizeof(Constant*)));
	v_hilo = builder->CreateShuffleVector(v1, v1, cv, "v128extractlow_shuffle");

	return builder->CreateBitCast(
		v_hilo,
		builder->getInt64Ty(), 
		"V128to64");
}
Value* VexExprUnopV128HIto64::emit(void) const
{
	Value		*v_hilo;
	Constant	*shuffle_v[] = {
		get_i32(8), get_i32(9),
		get_i32(10), get_i32(11),
		get_i32(12), get_i32(13),
		get_i32(14), get_i32(15),
	};
	Constant	*cv;

	UNOP_SETUP
	cv = ConstantVector::get(
		std::vector<Constant*>(
			shuffle_v,
			shuffle_v + sizeof(shuffle_v)/sizeof(Constant*)));
	v_hilo = builder->CreateShuffleVector(v1, v1, cv, "v128extracthi_shuffle");

	return builder->CreateBitCast(
		v_hilo,
		builder->getInt64Ty(), 
		"V128Hito64");
}

Value* VexExprBinop64HLto128::emit(void) const
{
	Value	*v_hi, *v_lo;

	BINOP_SETUP

	v_hi = builder->CreateZExt(
		v1, IntegerType::get(getGlobalContext(), 128));
	v_lo =  builder->CreateZExt(
		v2, IntegerType::get(getGlobalContext(), 128));

	return builder->CreateOr(v_lo, builder->CreateShl(v_hi, 64));
}

Value* VexExprUnop128HIto64::emit(void) const
{
	UNOP_SETUP
	return builder->CreateTrunc(
		builder->CreateLShr(
			builder->CreateBitCast(
				v1, 
				IntegerType::get(getGlobalContext(), 128)),
			64),
		builder->getInt64Ty());
}

/* so stupid */
Value* VexExprUnop32UtoV128::emit(void) const
{
	Value		*v_128i;

	UNOP_SETUP

	v_128i = builder->CreateZExt(
		v1, IntegerType::get(getGlobalContext(), 128));
	
	return builder->CreateBitCast(v_128i, get_vt_16x8(), "32UtoV128");
}

Value* VexExprUnop64UtoV128::emit(void) const
{
	Value		*v_128i;

	UNOP_SETUP

	v_128i = builder->CreateZExt(
		v1, IntegerType::get(getGlobalContext(), 128));
	
	return builder->CreateBitCast(v_128i, get_vt_16x8(), "64UtoV128");
}


Value* VexExprBinop64HLtoV128::emit(void) const
{
	Value		*v_hi, *v_lo, *v_128hl;

	BINOP_SETUP

	v_hi = builder->CreateZExt(
		v1, IntegerType::get(getGlobalContext(), 128));
	v_lo = builder->CreateZExt(
		v2, IntegerType::get(getGlobalContext(), 128));
	
	v_128hl = builder->CreateOr(
		v_lo,
		builder->CreateShl(v_hi, 64));

	return builder->CreateBitCast(v_128hl, get_vt_16x8(), "64HLtoV128");
}

/* (i32, i32) -> i64 */
Value* VexExprBinop32HLto64::emit(void) const
{
	Value		*v_hi, *v_lo;

	BINOP_SETUP

	v_hi = builder->CreateZExt(v1, builder->getInt64Ty());
	v_lo = builder->CreateZExt(v2, builder->getInt64Ty());

	return builder->CreateOr(
		v_lo,
		builder->CreateShl(v_hi, 32));
}

Value* VexExprBinop16HLto32::emit(void) const
{
	Value		*v_hi, *v_lo;

	BINOP_SETUP

	v_hi = builder->CreateZExt(v1, builder->getInt32Ty());
	v_lo = builder->CreateZExt(v2, builder->getInt32Ty());

	return builder->CreateOr(
		v_lo,
		builder->CreateShl(v_hi, 16));
}

#define BINOP_EMIT(x,y)				\
Value* VexExprBinop##x::emit(void) const	\
{						\
	BINOP_SETUP				\
	return builder->Create##y(v1,		\
		builder->CreateZExt(v2, v1->getType()));	\
}

BINOP_EMIT(Add8, Add)
BINOP_EMIT(Add16, Add)
BINOP_EMIT(Add32, Add)
BINOP_EMIT(Add64, Add)

BINOP_EMIT(And8, And)
BINOP_EMIT(And16, And)
BINOP_EMIT(And32, And)
BINOP_EMIT(And64, And)
BINOP_EMIT(AndV128, And)

BINOP_EMIT(Mul8, Mul)
BINOP_EMIT(Mul16, Mul)
BINOP_EMIT(Mul32, Mul)
BINOP_EMIT(Mul64, Mul)

#define BINOP_EXPAND_EMIT(x,y,z,w)				\
Value* VexExprBinop##x::emit(void) const			\
{								\
	BINOP_SETUP						\
	v1 = builder->Create##y(v1, z);				\
	v2 = builder->Create##y(v2, z);				\
	return builder->Create##w(v1, v2);			\
}

BINOP_EXPAND_EMIT(MullS8, SExt, builder->getInt16Ty(), Mul)
BINOP_EXPAND_EMIT(MullS16, SExt, builder->getInt32Ty(), Mul)
BINOP_EXPAND_EMIT(MullS32, SExt, builder->getInt64Ty(), Mul)
BINOP_EXPAND_EMIT(MullS64, SExt, IntegerType::get(getGlobalContext(), 128),Mul)

BINOP_EXPAND_EMIT(MullU8, ZExt, builder->getInt16Ty(), Mul)
BINOP_EXPAND_EMIT(MullU16, ZExt, builder->getInt32Ty(), Mul)
BINOP_EXPAND_EMIT(MullU32, ZExt, builder->getInt64Ty(), Mul)
BINOP_EXPAND_EMIT(MullU64, ZExt, IntegerType::get(getGlobalContext(),128), Mul)

BINOP_EMIT(Or8, Or)
BINOP_EMIT(Or16, Or)
BINOP_EMIT(Or32, Or)
BINOP_EMIT(Or64, Or)
BINOP_EMIT(OrV128, Or)

BINOP_EMIT(Shl8, Shl)
BINOP_EMIT(Shl16, Shl)
BINOP_EMIT(Shl32, Shl)
BINOP_EMIT(Shl64, Shl)

BINOP_EMIT(Shr8, LShr)
BINOP_EMIT(Shr16, LShr)
BINOP_EMIT(Shr32, LShr)
BINOP_EMIT(Shr64, LShr)

BINOP_EMIT(Sar8, AShr)
BINOP_EMIT(Sar16, AShr)
BINOP_EMIT(Sar32, AShr)
BINOP_EMIT(Sar64, AShr)

BINOP_EMIT(Sub8, Sub)
BINOP_EMIT(Sub16, Sub)
BINOP_EMIT(Sub32, Sub)
BINOP_EMIT(Sub64, Sub)

BINOP_EMIT(Xor8, Xor)
BINOP_EMIT(Xor16, Xor)
BINOP_EMIT(Xor32, Xor)
BINOP_EMIT(Xor64, Xor)
BINOP_EMIT(XorV128, Xor)

BINOP_EMIT(CmpEQ8, ICmpEQ)
BINOP_EMIT(CmpEQ16, ICmpEQ)
BINOP_EMIT(CmpEQ32, ICmpEQ)
BINOP_EMIT(CmpEQ64, ICmpEQ)
BINOP_EMIT(CmpNE8, ICmpNE)
BINOP_EMIT(CmpNE16, ICmpNE)
BINOP_EMIT(CmpNE32, ICmpNE)
BINOP_EMIT(CmpNE64, ICmpNE)

BINOP_EMIT(CasCmpEQ8, ICmpEQ)
BINOP_EMIT(CasCmpEQ16, ICmpEQ)
BINOP_EMIT(CasCmpEQ32, ICmpEQ)
BINOP_EMIT(CasCmpEQ64, ICmpEQ)
BINOP_EMIT(CasCmpNE8, ICmpNE)
BINOP_EMIT(CasCmpNE16, ICmpNE)
BINOP_EMIT(CasCmpNE32, ICmpNE)
BINOP_EMIT(CasCmpNE64, ICmpNE)

#define DIVMOD_EMIT(x,y,z,w)			\
Value* VexExprBinop##x::emit(void) const	\
{	\
	Value	*div, *mod;	\
	BINOP_SETUP	\
	v1 = builder->CreateBitCast(v1, 			\
		IntegerType::get(getGlobalContext(), w));	\
	v2 = builder->Create##z##Ext(v2, 			\
		IntegerType::get(getGlobalContext(), w));	\
	div = builder->Create##z##Ext(				\
		builder->Create##y##Div(v1, v2),		\
		IntegerType::get(getGlobalContext(), w));	\
	mod = builder->Create##z##Ext(				\
		builder->Create##y##Rem(v1, v2),		\
		IntegerType::get(getGlobalContext(), w));	\
	return builder->CreateOr(	\
		div,			\
		builder->CreateShl(mod, w/2));		\
}

DIVMOD_EMIT(DivModU128to64, U, Z, 128)
DIVMOD_EMIT(DivModS128to64, S, S, 128)
DIVMOD_EMIT(DivModU64to32, U, Z, 64)
DIVMOD_EMIT(DivModS64to32, S, S, 64)

#define OPF0X_EMIT(x, y, z)			\
Value* VexExprBinop##x::emit(void) const	\
{	\
	Value	*lo_op_lhs, *lo_op_rhs, *result;	\
	BINOP_SETUP					\
	v1 = builder->CreateBitCast(v1, y);		\
	v2 = builder->CreateBitCast(v2, y);		\
	lo_op_lhs = builder->CreateExtractElement(v1, get_i32(0));	\
	lo_op_rhs = builder->CreateExtractElement(v2, get_i32(0));	\
	result = builder->Create##z(lo_op_lhs, lo_op_rhs);		\
	return builder->CreateInsertElement(v1, result, get_i32(0));	\
}

OPF0X_EMIT(Mul64F0x2, get_vt_2xf64(), FMul)
OPF0X_EMIT(Div64F0x2, get_vt_2xf64(), FDiv)
OPF0X_EMIT(Add64F0x2, get_vt_2xf64(), FAdd)
OPF0X_EMIT(Sub64F0x2, get_vt_2xf64(), FSub)
OPF0X_EMIT(Mul32F0x4, get_vt_4xf32(), FMul)
OPF0X_EMIT(Div32F0x4, get_vt_4xf32(), FDiv)
OPF0X_EMIT(Add32F0x4, get_vt_4xf32(), FAdd)
OPF0X_EMIT(Sub32F0x4, get_vt_4xf32(), FSub)

Value* VexExprBinopCmpLT32F0x4::emit(void) const
{
	Value	*lo_op_lhs, *lo_op_rhs, *result;
	BINOP_SETUP
	v1 = builder->CreateBitCast(v1, get_vt_4xf32());
	v2 = builder->CreateBitCast(v2, get_vt_4xf32());
	lo_op_lhs = builder->CreateExtractElement(v1, get_i32(0));
	lo_op_rhs = builder->CreateExtractElement(v2, get_i32(0));
	result = builder->CreateFCmpOLT(lo_op_lhs, lo_op_rhs);
	result = builder->CreateSExt(result, builder->getInt32Ty());
	result = builder->CreateBitCast(result, builder->getFloatTy());

	return builder->CreateInsertElement(
		builder->CreateBitCast(v1, get_vt_4x32()),
		result,
		get_i32(0));
}

Value* VexExprBinopMax32F0x4::emit(void) const
{
	Value	*lo_op_lhs, *lo_op_rhs, *result;
	Function	*f;
	BINOP_SETUP
	v1 = builder->CreateBitCast(v1, get_vt_4xf32());
	v2 = builder->CreateBitCast(v2, get_vt_4xf32());
	lo_op_lhs = builder->CreateExtractElement(v1, get_i32(0));
	lo_op_rhs = builder->CreateExtractElement(v2, get_i32(0));
	f = theVexHelpers->getHelper("vexop_maxf32");
	assert (f != NULL);
	result = builder->CreateCall2(f, lo_op_lhs, lo_op_rhs);
	return builder->CreateInsertElement(v1, result, get_i32(0));
}

Value* VexExprBinopCmpEQ8x16::emit(void) const
{
	Value		*cmp_8x1;
	BINOP_SETUP
	cmp_8x1 = builder->CreateICmpEQ(v1, v2);
	return builder->CreateSExt(cmp_8x1, get_vt_16x8());
}

Value* VexExprBinopCmpGT8Sx16::emit(void) const
{
	Value		*cmp_8x1;
	BINOP_SETUP
	cmp_8x1 = builder->CreateICmpSGT(v1, v2);
	return builder->CreateSExt(cmp_8x1, get_vt_16x8());
}

Value* VexExprBinopSub8x16::emit(void) const
{
	BINOP_SETUP
	v1 = builder->CreateBitCast(v1, get_vt_16x8());
	v2 = builder->CreateBitCast(v2, get_vt_16x8());
	return builder->CreateSub(v1, v2);
}

#define EMIT_HELPER_BINOP(x,y)			\
Value* VexExprBinop##x::emit(void) const	\
{						\
	llvm::Function	*f;			\
	BINOP_SETUP				\
	f = theVexHelpers->getHelper(y);	\
	assert (f != NULL);	\
	return builder->CreateCall2(f, v1, v2);	\
}

EMIT_HELPER_BINOP(CmpF64, "vexop_cmpf64")

BINOP_EMIT(CmpLE64S, ICmpSLE)
BINOP_EMIT(CmpLE64U, ICmpULE)
BINOP_EMIT(CmpLT64S, ICmpSLT)
BINOP_EMIT(CmpLT64U, ICmpULT)
BINOP_EMIT(CmpLE32S, ICmpSLE)
BINOP_EMIT(CmpLE32U, ICmpULE)
BINOP_EMIT(CmpLT32S, ICmpSLT)
BINOP_EMIT(CmpLT32U, ICmpULT)

#define EMIT_HELPER_UNOP(x,y)			\
Value* VexExprUnop##x::emit(void) const	\
{						\
	IRBuilder<>     *builder = theGenLLVM->getBuilder();	\
	llvm::Function	*f;	\
	llvm::Value	*v;	\
	v = args[0]->emit();	\
	f = theVexHelpers->getHelper(y);	\
	assert (f != NULL);	\
	return builder->CreateCall(f, v);	\
}

/* count number of zeros (from bit0) leading up to first 1. */
/* 0x0 -> undef */
/* 0x1 -> 1 */
/* 0x2 -> 2 */
/* 0x3 -> 1 */
/* 0x4 -> 3 */
EMIT_HELPER_UNOP(Ctz64, "vexop_ctz64")
EMIT_HELPER_UNOP(Clz64, "vexop_clz64")

/* interleave 16 elements of an 8-bit width */
Value* VexExprBinopInterleaveLO8x16::emit(void) const
{
	Constant	*shuffle_v[] = {
		get_i32(0), get_i32(16),
		get_i32(1), get_i32(17),
		get_i32(2), get_i32(18),
		get_i32(3), get_i32(19),
		get_i32(4), get_i32(20),
		get_i32(5), get_i32(21),
		get_i32(6), get_i32(22),
		get_i32(7), get_i32(23)};
	Constant	*cv;

	BINOP_SETUP

	v1 = builder->CreateBitCast(v1, get_vt_16x8(), "lo8x16_v1");
	v2 = builder->CreateBitCast(v2, get_vt_16x8(), "lo8x16_v2");

	cv = ConstantVector::get(
		std::vector<Constant*>(
			shuffle_v,
			shuffle_v + sizeof(shuffle_v)/sizeof(Constant*)));

	return builder->CreateShuffleVector(v1, v2, cv, "ILO8x16");
}
