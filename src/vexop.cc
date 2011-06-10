#include <stdio.h>
#include <iostream>
#include "genllvm.h"
#include "vexop.h"
#include "vexhelpers.h"
#include <llvm/Intrinsics.h>

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

CASE_OP(InterleaveLO64x2)

CASE_OP(Shl8x8)
CASE_OP(Shl16x4)
CASE_OP(Shl32x2)
CASE_OP(Shr8x8)
CASE_OP(Shr16x4)
CASE_OP(Shr32x2)
CASE_OP(Sar8x8) 
CASE_OP(Sar16x4)
CASE_OP(Sar32x2)
CASE_OP(Sal8x8) 
CASE_OP(Sal16x4)
CASE_OP(Sal32x2)
CASE_OP(ShlN8x8)
CASE_OP(ShlN16x4)
CASE_OP(ShlN32x2)
CASE_OP(ShrN8x8)
CASE_OP(ShrN16x4)
CASE_OP(ShrN32x2)
CASE_OP(SarN8x8)
CASE_OP(SarN16x4)
CASE_OP(SarN32x2)

CASE_OP(Perm8x8)
CASE_OP(Perm8x16)

CASE_OP(ReinterpF64asI64)
CASE_OP(ReinterpI64asF64)
CASE_OP(ReinterpF32asI32)
CASE_OP(ReinterpI32asF32)


CASE_OP(F64toI16S)
CASE_OP(F64toI32S)
CASE_OP(F64toI64S)
CASE_OP(F64toI32U)
CASE_OP(I16StoF64)
CASE_OP(I32StoF64)
CASE_OP(I64StoF64)
CASE_OP(I32UtoF64)
CASE_OP(F32toF64)
CASE_OP(F64toF32)
CASE_OP(AtanF64)
CASE_OP(Yl2xF64)
CASE_OP(Yl2xp1F64)
CASE_OP(PRemF64)
CASE_OP(PRemC3210F64)
CASE_OP(PRem1F64)
CASE_OP(PRem1C3210F64)
CASE_OP(ScaleF64)
CASE_OP(SinF64)
CASE_OP(CosF64)
CASE_OP(TanF64)
CASE_OP(2xm1F64)
CASE_OP(RoundF64toInt)
CASE_OP(RoundF32toInt)
CASE_OP(MAddF64)
CASE_OP(MSubF64)
CASE_OP(MAddF64r32)
CASE_OP(MSubF64r32)
CASE_OP(Est5FRSqrt)
CASE_OP(RoundF64toF64_NEAREST)
CASE_OP(RoundF64toF64_NegINF)
CASE_OP(RoundF64toF64_PosINF)
CASE_OP(RoundF64toF64_ZERO)
CASE_OP(TruncF64asF32)
CASE_OP(RoundF64toF32)
CASE_OP(CalcFPRF)
CASE_OP(Add16x2)
CASE_OP(Sub16x2)
CASE_OP(QAdd16Sx2)
CASE_OP(QAdd16Ux2)
CASE_OP(QSub16Sx2)
CASE_OP(QSub16Ux2)
CASE_OP(HAdd16Ux2)
CASE_OP(HAdd16Sx2)
CASE_OP(HSub16Ux2)
CASE_OP(HSub16Sx2)
CASE_OP(Add8x4)
CASE_OP(Sub8x4)
CASE_OP(QAdd8Sx4)
CASE_OP(QAdd8Ux4)
CASE_OP(QSub8Sx4)
CASE_OP(QSub8Ux4)
CASE_OP(HAdd8Ux4)
CASE_OP(HAdd8Sx4)
CASE_OP(HSub8Ux4)
CASE_OP(HSub8Sx4)
CASE_OP(Sad8Ux4)
CASE_OP(CmpNEZ16x2)
CASE_OP(CmpNEZ8x4)
CASE_OP(I32UtoFx2)
CASE_OP(I32StoFx2)
CASE_OP(FtoI32Ux2_RZ)
CASE_OP(FtoI32Sx2_RZ)
CASE_OP(F32ToFixed32Ux2_RZ)
CASE_OP(F32ToFixed32Sx2_RZ)
CASE_OP(Fixed32UToF32x2_RN)
CASE_OP(Fixed32SToF32x2_RN)
CASE_OP(Max32Fx2)
CASE_OP(Min32Fx2)
CASE_OP(PwMax32Fx2)
CASE_OP(PwMin32Fx2)
CASE_OP(CmpEQ32Fx2)
CASE_OP(CmpGT32Fx2)
CASE_OP(CmpGE32Fx2)
CASE_OP(Recip32Fx2)
CASE_OP(Recps32Fx2)
CASE_OP(Rsqrte32Fx2)
CASE_OP(Rsqrts32Fx2)
CASE_OP(Neg32Fx2)
CASE_OP(Abs32Fx2)
CASE_OP(CmpNEZ8x8)
CASE_OP(CmpNEZ16x4)
CASE_OP(CmpNEZ32x2)
CASE_OP(Add8x8)
CASE_OP(Add16x4)
CASE_OP(Add32x2)
CASE_OP(QAdd8Ux8)
CASE_OP(QAdd16Ux4)
CASE_OP(QAdd32Ux2)
CASE_OP(QAdd64Ux1)
CASE_OP(QAdd8Sx8)
CASE_OP(QAdd16Sx4)
CASE_OP(QAdd32Sx2)
CASE_OP(QAdd64Sx1)
CASE_OP(PwAdd8x8)
CASE_OP(PwAdd16x4)
CASE_OP(PwAdd32x2)
CASE_OP(PwMax8Sx8)
CASE_OP(PwMax16Sx4)
CASE_OP(PwMax32Sx2)
CASE_OP(PwMax8Ux8)
CASE_OP(PwMax16Ux4)
CASE_OP(PwMax32Ux2)
CASE_OP(PwMin8Sx8)
CASE_OP(PwMin16Sx4)
CASE_OP(PwMin32Sx2)
CASE_OP(PwMin8Ux8)
CASE_OP(PwMin16Ux4)
CASE_OP(PwMin32Ux2)
CASE_OP(PwAddL8Ux8)
CASE_OP(PwAddL16Ux4)
CASE_OP(PwAddL32Ux2)
CASE_OP(PwAddL8Sx8)
CASE_OP(PwAddL16Sx4)
CASE_OP(PwAddL32Sx2)
CASE_OP(Sub8x8)
CASE_OP(Sub16x4)
CASE_OP(Sub32x2)
CASE_OP(QSub8Ux8)
CASE_OP(QSub16Ux4)
CASE_OP(QSub32Ux2)
CASE_OP(QSub64Ux1)
CASE_OP(QSub8Sx8)
CASE_OP(QSub16Sx4)
CASE_OP(QSub32Sx2)
CASE_OP(QSub64Sx1)
CASE_OP(Abs8x8)
CASE_OP(Abs16x4)
CASE_OP(Abs32x2)
CASE_OP(Mul8x8)
CASE_OP(Mul16x4)
CASE_OP(Mul32x2)
CASE_OP(Mul32Fx2)
CASE_OP(MulHi16Ux4)
CASE_OP(MulHi16Sx4)
CASE_OP(PolynomialMul8x8)
CASE_OP(QDMulHi16Sx4)
CASE_OP(QDMulHi32Sx2)
CASE_OP(QRDMulHi16Sx4)
CASE_OP(QRDMulHi32Sx2)
CASE_OP(Avg8Ux8)
CASE_OP(Avg16Ux4)
CASE_OP(Max8Sx8)
CASE_OP(Max16Sx4)
CASE_OP(Max32Sx2)
CASE_OP(Max8Ux8)
CASE_OP(Max16Ux4)
CASE_OP(Max32Ux2)
CASE_OP(Min8Sx8)
CASE_OP(Min16Sx4)
CASE_OP(Min32Sx2)
CASE_OP(Min8Ux8)
CASE_OP(Min16Ux4)
CASE_OP(Min32Ux2)
CASE_OP(CmpEQ8x8)
CASE_OP(CmpEQ16x4)
CASE_OP(CmpEQ32x2)
CASE_OP(CmpGT8Ux8)
CASE_OP(CmpGT16Ux4)
CASE_OP(CmpGT32Ux2)
CASE_OP(CmpGT8Sx8)
CASE_OP(CmpGT16Sx4)
CASE_OP(CmpGT32Sx2)
CASE_OP(Cnt8x8)
CASE_OP(Clz8Sx8)
CASE_OP(Clz16Sx4)
CASE_OP(Clz32Sx2)
CASE_OP(Cls8Sx8)
CASE_OP(Cls16Sx4)
CASE_OP(Cls32Sx2)
CASE_OP(Sal64x1)
CASE_OP(QShl8x8)
CASE_OP(QShl16x4)
CASE_OP(QShl32x2)
CASE_OP(QShl64x1)
CASE_OP(QSal8x8)
CASE_OP(QSal16x4)
CASE_OP(QSal32x2)
CASE_OP(QSal64x1)
CASE_OP(QShlN8Sx8)
CASE_OP(QShlN16Sx4)
CASE_OP(QShlN32Sx2)
CASE_OP(QShlN64Sx1)
CASE_OP(QShlN8x8)
CASE_OP(QShlN16x4)
CASE_OP(QShlN32x2)
CASE_OP(QShlN64x1)
CASE_OP(QSalN8x8)
CASE_OP(QSalN16x4)
CASE_OP(QSalN32x2)
CASE_OP(QSalN64x1)
CASE_OP(QNarrow16Ux4)
CASE_OP(QNarrow16Sx4)
CASE_OP(QNarrow32Sx2)
CASE_OP(InterleaveHI8x8)
CASE_OP(InterleaveHI16x4)
CASE_OP(InterleaveHI32x2)
CASE_OP(InterleaveLO8x8)
CASE_OP(InterleaveLO16x4)
CASE_OP(InterleaveLO32x2)
CASE_OP(InterleaveOddLanes8x8)
CASE_OP(InterleaveEvenLanes8x8)
CASE_OP(InterleaveOddLanes16x4)
CASE_OP(InterleaveEvenLanes16x4)
CASE_OP(CatOddLanes8x8)
CASE_OP(CatOddLanes16x4)
CASE_OP(CatEvenLanes8x8)
CASE_OP(CatEvenLanes16x4)
CASE_OP(GetElem8x8)
CASE_OP(GetElem16x4)
CASE_OP(GetElem32x2)
CASE_OP(SetElem8x8)
CASE_OP(SetElem16x4)
CASE_OP(SetElem32x2)
CASE_OP(Dup8x8)
CASE_OP(Dup16x4)
CASE_OP(Dup32x2)
CASE_OP(Extract64)
CASE_OP(Reverse16_8x8)
CASE_OP(Reverse32_8x8)
CASE_OP(Reverse32_16x4)
CASE_OP(Reverse64_8x8)
CASE_OP(Reverse64_16x4)
CASE_OP(Reverse64_32x2)
CASE_OP(Recip32x2)
CASE_OP(Rsqrte32x2)
CASE_OP(Add32Fx4)
CASE_OP(Sub32Fx4)
CASE_OP(Mul32Fx4)
CASE_OP(Div32Fx4)
CASE_OP(Max32Fx4)
CASE_OP(Min32Fx4)
CASE_OP(Add32Fx2)
CASE_OP(Sub32Fx2)
CASE_OP(CmpEQ32Fx4)
CASE_OP(CmpLT32Fx4)
CASE_OP(CmpLE32Fx4)
CASE_OP(CmpUN32Fx4)
CASE_OP(CmpGT32Fx4)
CASE_OP(CmpGE32Fx4)
CASE_OP(Abs32Fx4)
CASE_OP(PwMax32Fx4)
CASE_OP(PwMin32Fx4)
CASE_OP(Sqrt32Fx4)
CASE_OP(RSqrt32Fx4)
CASE_OP(Neg32Fx4)
CASE_OP(Recip32Fx4)
CASE_OP(Recps32Fx4)
CASE_OP(Rsqrte32Fx4)
CASE_OP(Rsqrts32Fx4)
CASE_OP(I32UtoFx4)
CASE_OP(I32StoFx4)
CASE_OP(FtoI32Ux4_RZ)
CASE_OP(FtoI32Sx4_RZ)
CASE_OP(QFtoI32Ux4_RZ)
CASE_OP(QFtoI32Sx4_RZ)
CASE_OP(RoundF32x4_RM)
CASE_OP(RoundF32x4_RP)
CASE_OP(RoundF32x4_RN)
CASE_OP(RoundF32x4_RZ)
CASE_OP(F32ToFixed32Ux4_RZ)
CASE_OP(F32ToFixed32Sx4_RZ)
CASE_OP(Fixed32UToF32x4_RN)
CASE_OP(Fixed32SToF32x4_RN)
CASE_OP(F32toF16x4)
CASE_OP(F16toF32x4)
CASE_OP(Add32F0x4)
CASE_OP(Sub32F0x4)
CASE_OP(Mul32F0x4)
CASE_OP(Div32F0x4)
CASE_OP(Max32F0x4)
CASE_OP(Min32F0x4)
CASE_OP(CmpEQ32F0x4)
CASE_OP(CmpLT32F0x4)
CASE_OP(CmpLE32F0x4)
CASE_OP(CmpUN32F0x4)
CASE_OP(Recip32F0x4)
CASE_OP(Sqrt32F0x4)
CASE_OP(RSqrt32F0x4)
CASE_OP(V128HIto64)
CASE_OP(64UtoV128)
CASE_OP(SetV128lo64)
CASE_OP(V128to32)
CASE_OP(SetV128lo32)
CASE_OP(NotV128)
CASE_OP(AndV128)
CASE_OP(OrV128)
CASE_OP(XorV128)
CASE_OP(ShlV128)
CASE_OP(ShrV128)
CASE_OP(CmpNEZ8x16)
CASE_OP(CmpNEZ16x8)
CASE_OP(CmpNEZ32x4)
CASE_OP(CmpNEZ64x2)
CASE_OP(Add8x16)
CASE_OP(Add16x8)
CASE_OP(Add32x4)
CASE_OP(Add64x2)
CASE_OP(QAdd8Ux16)
CASE_OP(QAdd16Ux8)
CASE_OP(QAdd32Ux4)
CASE_OP(QAdd64Ux2)
CASE_OP(QAdd8Sx16)
CASE_OP(QAdd16Sx8)
CASE_OP(QAdd32Sx4)
CASE_OP(QAdd64Sx2)
CASE_OP(Sub8x16)
CASE_OP(Sub16x8)
CASE_OP(Sub32x4)
CASE_OP(Sub64x2)
CASE_OP(QSub8Ux16)
CASE_OP(QSub16Ux8)
CASE_OP(QSub32Ux4)
CASE_OP(QSub64Ux2)
CASE_OP(QSub8Sx16)
CASE_OP(QSub16Sx8)
CASE_OP(QSub32Sx4)
CASE_OP(QSub64Sx2)
CASE_OP(Mul8x16)
CASE_OP(Mul16x8)
CASE_OP(Mul32x4)
CASE_OP(MulHi16Ux8)
CASE_OP(MulHi32Ux4)
CASE_OP(MulHi16Sx8)
CASE_OP(MulHi32Sx4)
CASE_OP(MullEven8Ux16)
CASE_OP(MullEven16Ux8)
CASE_OP(MullEven8Sx16)
CASE_OP(MullEven16Sx8)
CASE_OP(Mull8Ux8)
CASE_OP(Mull8Sx8)
CASE_OP(Mull16Ux4)
CASE_OP(Mull16Sx4)
CASE_OP(Mull32Ux2)
CASE_OP(Mull32Sx2)
CASE_OP(QDMulHi16Sx8)
CASE_OP(QDMulHi32Sx4)
CASE_OP(QRDMulHi16Sx8)
CASE_OP(QRDMulHi32Sx4)
CASE_OP(QDMulLong16Sx4)
CASE_OP(QDMulLong32Sx2)
CASE_OP(PolynomialMul8x16)
CASE_OP(PolynomialMull8x8)
CASE_OP(PwAdd8x16)
CASE_OP(PwAdd16x8)
CASE_OP(PwAdd32x4)
CASE_OP(PwAdd32Fx2)
CASE_OP(PwAddL8Ux16)
CASE_OP(PwAddL16Ux8)
CASE_OP(PwAddL32Ux4)
CASE_OP(PwAddL8Sx16)
CASE_OP(PwAddL16Sx8)
CASE_OP(PwAddL32Sx4)
CASE_OP(Abs8x16)
CASE_OP(Abs16x8)
CASE_OP(Abs32x4)
CASE_OP(Avg8Ux16)
CASE_OP(Avg16Ux8)
CASE_OP(Avg32Ux4)
CASE_OP(Avg8Sx16)
CASE_OP(Avg16Sx8)
CASE_OP(Avg32Sx4)
CASE_OP(Max8Sx16)
CASE_OP(Max16Sx8)
CASE_OP(Max32Sx4)
CASE_OP(Max8Ux16)
CASE_OP(Max16Ux8)
CASE_OP(Max32Ux4)
CASE_OP(Min8Sx16)
CASE_OP(Min16Sx8)
CASE_OP(Min32Sx4)
CASE_OP(Min8Ux16)
CASE_OP(Min16Ux8)
CASE_OP(Min32Ux4)
CASE_OP(CmpEQ8x16)
CASE_OP(CmpEQ16x8)
CASE_OP(CmpEQ32x4)
CASE_OP(CmpGT8Sx16)
CASE_OP(CmpGT16Sx8)
CASE_OP(CmpGT32Sx4)
CASE_OP(CmpGT64Sx2)
CASE_OP(CmpGT8Ux16)
CASE_OP(CmpGT16Ux8)
CASE_OP(CmpGT32Ux4)
CASE_OP(Cnt8x16)
CASE_OP(Clz8Sx16)
CASE_OP(Clz16Sx8)
CASE_OP(Clz32Sx4)
CASE_OP(Cls8Sx16)
CASE_OP(Cls16Sx8)
CASE_OP(Cls32Sx4)
CASE_OP(ShlN8x16)
CASE_OP(ShlN16x8)
CASE_OP(ShlN32x4)
CASE_OP(ShlN64x2)
CASE_OP(ShrN8x16)
CASE_OP(ShrN16x8)
CASE_OP(ShrN32x4)
CASE_OP(ShrN64x2)
CASE_OP(SarN8x16)
CASE_OP(SarN16x8)
CASE_OP(SarN32x4)
CASE_OP(SarN64x2)
CASE_OP(Shl8x16)
CASE_OP(Shl16x8)
CASE_OP(Shl32x4)
CASE_OP(Shl64x2)
CASE_OP(Shr8x16)
CASE_OP(Shr16x8)
CASE_OP(Shr32x4)
CASE_OP(Shr64x2)
CASE_OP(Sar8x16)
CASE_OP(Sar16x8)
CASE_OP(Sar32x4)
CASE_OP(Sar64x2)
CASE_OP(Sal8x16)
CASE_OP(Sal16x8)
CASE_OP(Sal32x4)
CASE_OP(Sal64x2)
CASE_OP(Rol8x16)
CASE_OP(Rol16x8)
CASE_OP(Rol32x4)
CASE_OP(QShl8x16)
CASE_OP(QShl16x8)
CASE_OP(QShl32x4)
CASE_OP(QShl64x2)
CASE_OP(QSal8x16)
CASE_OP(QSal16x8)
CASE_OP(QSal32x4)
CASE_OP(QSal64x2)
CASE_OP(QShlN8Sx16)
CASE_OP(QShlN16Sx8)
CASE_OP(QShlN32Sx4)
CASE_OP(QShlN64Sx2)
CASE_OP(QShlN8x16)
CASE_OP(QShlN16x8)
CASE_OP(QShlN32x4)
CASE_OP(QShlN64x2)
CASE_OP(QSalN8x16)
CASE_OP(QSalN16x8)
CASE_OP(QSalN32x4)
CASE_OP(QSalN64x2)
CASE_OP(QNarrow16Ux8)
CASE_OP(QNarrow32Ux4)
CASE_OP(QNarrow16Sx8)
CASE_OP(QNarrow32Sx4)
CASE_OP(Narrow16x8)
CASE_OP(Narrow32x4)
CASE_OP(Shorten16x8)
CASE_OP(Shorten32x4)
CASE_OP(Shorten64x2)
CASE_OP(QShortenS16Sx8)
CASE_OP(QShortenS32Sx4)
CASE_OP(QShortenS64Sx2)
CASE_OP(QShortenU16Sx8)
CASE_OP(QShortenU32Sx4)
CASE_OP(QShortenU64Sx2)
CASE_OP(QShortenU16Ux8)
CASE_OP(QShortenU32Ux4)
CASE_OP(QShortenU64Ux2)
CASE_OP(Longen8Ux8)
CASE_OP(Longen16Ux4)
CASE_OP(Longen32Ux2)
CASE_OP(Longen8Sx8)
CASE_OP(Longen16Sx4)
CASE_OP(Longen32Sx2)
CASE_OP(InterleaveHI8x16)
CASE_OP(InterleaveHI16x8)
CASE_OP(InterleaveHI32x4)
CASE_OP(InterleaveHI64x2)
CASE_OP(InterleaveLO16x8)
CASE_OP(InterleaveLO32x4)
CASE_OP(InterleaveOddLanes8x16)
CASE_OP(InterleaveEvenLanes8x16)
CASE_OP(InterleaveOddLanes16x8)
CASE_OP(InterleaveEvenLanes16x8)
CASE_OP(InterleaveOddLanes32x4)
CASE_OP(InterleaveEvenLanes32x4)
CASE_OP(CatOddLanes8x16)
CASE_OP(CatOddLanes16x8)
CASE_OP(CatOddLanes32x4)
CASE_OP(CatEvenLanes8x16)
CASE_OP(CatEvenLanes16x8)
CASE_OP(CatEvenLanes32x4)
CASE_OP(GetElem8x16)
CASE_OP(GetElem16x8)
CASE_OP(GetElem32x4)
CASE_OP(GetElem64x2)
CASE_OP(Dup8x16)
CASE_OP(Dup16x8)
CASE_OP(Dup32x4)
CASE_OP(ExtractV128)
CASE_OP(Reverse16_8x16)
CASE_OP(Reverse32_8x16)
CASE_OP(Reverse32_16x8)
CASE_OP(Reverse64_8x16)
CASE_OP(Reverse64_16x8)
CASE_OP(Reverse64_32x4)
CASE_OP(Recip32x4)
CASE_OP(Rsqrte32x4)

	case Iop_INVALID: return "Iop_INVALID";
	default:
	fprintf(stderr, "Really unknown opcode %x\n", op);
	}
	return "???Op???";
}

#define get_i(b) IntegerType::get(getGlobalContext(), b)
#define get_vt(w, b) VectorType::get(get_i(b), w)
#define get_c(b, v) ConstantInt::get(getGlobalContext(), APInt(b, v))
#define get_32i(v)	get_c(32, v)
#define get_f() Type::getFloatTy(getGlobalContext())
#define get_d() Type::getDoubleTy(getGlobalContext())
#define get_vtf(w) VectorType::get(builder->getFloatTy(), w)
#define get_vtd(w) VectorType::get(builder->getDoubleTy(), w)
#define get_cvl(var, l) 						\
	ConstantVector::get(std::vector<Constant*>(			\
		var, var + l))	
#define get_cv(var) get_cvl(var, sizeof(var)/sizeof(Constant*))


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

#define TRIOP_SETUP				\
	Value		*v1, *v2, *v3;		\
	IRBuilder<>	*builder;		\
	v1 = args[0]->emit();			\
	v2 = args[1]->emit();			\
	v3 = args[2]->emit();			\
	builder = theGenLLVM->getBuilder();

#define UNOP_EMIT(x,y)				\
Value* VexExprUnop##x::emit(void) const	\
{						\
	UNOP_SETUP				\
	return builder->y(v1);			\
}

#define X_TO_Y_EMIT(x,y,w,z)			\
Value* VexExprUnop##x::emit(void) const		\
{						\
	UNOP_SETUP				\
	v1 = builder->CreateBitCast(v1, w);	\
	return builder->y(v1, z);		\
}

/* XXX probably wrong to discard rounding info, but who cares */
#define X_TO_Y_EMIT_ROUND(x,y,w,z)		\
Value* VexExprBinop##x::emit(void) const	\
{						\
	BINOP_SETUP				\
	v2 = builder->CreateBitCast(v2, w);	\
	return builder->y(v2, z);		\
}

#define XHI_TO_Y_EMIT(x,y,w,z)				\
Value* VexExprUnop##x::emit(void) const			\
{							\
	UNOP_SETUP					\
	v1 = builder->CreateBitCast(v1, w);		\
	return builder->CreateTrunc(			\
		builder->CreateLShr(v1, get_c(y*2,y)),	\
		z);					\
}							\

XHI_TO_Y_EMIT(64HIto32, 32, get_i(64), get_i(32))
XHI_TO_Y_EMIT(32HIto16, 16, get_i(32), get_i(16))

X_TO_Y_EMIT(16to8,    CreateTrunc, get_i(16), get_i(8))
X_TO_Y_EMIT(64to32,   CreateTrunc, get_i(64), get_i(32))
X_TO_Y_EMIT(32Uto64,  CreateZExt,  get_i(32), get_i(64))
X_TO_Y_EMIT(32Sto64,  CreateSExt,  get_i(32), get_i(64))
X_TO_Y_EMIT(32to8,    CreateTrunc, get_i(32), get_i(8))
X_TO_Y_EMIT(32to16,   CreateTrunc, get_i(32), get_i(16))
X_TO_Y_EMIT(64to1,    CreateTrunc, get_i(64), get_i(1))
X_TO_Y_EMIT(64to8,    CreateTrunc, get_i(64), get_i(8))
X_TO_Y_EMIT(64to16,   CreateTrunc, get_i(64), get_i(16))
X_TO_Y_EMIT(1Uto8,    CreateZExt,  get_i(1),  get_i(8))
X_TO_Y_EMIT(1Uto64,   CreateZExt,  get_i(1),  get_i(64))
X_TO_Y_EMIT(16Uto64,  CreateZExt,  get_i(16), get_i(64))
X_TO_Y_EMIT(16Sto64,  CreateSExt,  get_i(16), get_i(64))
X_TO_Y_EMIT(16Uto32,  CreateZExt,  get_i(16), get_i(32))
X_TO_Y_EMIT(16Sto32,  CreateSExt,  get_i(16), get_i(32))
X_TO_Y_EMIT(8Uto16,   CreateZExt,  get_i(8),  get_i(16))
X_TO_Y_EMIT(8Sto16,   CreateSExt,  get_i(8),  get_i(16))
X_TO_Y_EMIT(8Uto32,   CreateZExt,  get_i(8),  get_i(32))
X_TO_Y_EMIT(8Sto32,   CreateSExt,  get_i(8),  get_i(32))
X_TO_Y_EMIT(8Uto64,   CreateZExt,  get_i(8),  get_i(64))
X_TO_Y_EMIT(8Sto64,   CreateSExt,  get_i(8),  get_i(64))
X_TO_Y_EMIT(128to64,  CreateTrunc, get_i(128), get_i(64))
X_TO_Y_EMIT(F32toF64, CreateFPExt, get_f(),    get_d())

X_TO_Y_EMIT(I32StoF64, CreateSIToFP, get_i(32), get_d())

X_TO_Y_EMIT_ROUND(F64toF32, CreateFPTrunc, get_d(), get_f())
X_TO_Y_EMIT_ROUND(I64StoF64, CreateSIToFP, get_i(64), get_d())
X_TO_Y_EMIT_ROUND(I64UtoF64, CreateSIToFP, get_i(64), get_d())
X_TO_Y_EMIT_ROUND(F64toI32S, CreateFPToSI, get_d(), get_i(32))
X_TO_Y_EMIT_ROUND(F64toI32U, CreateFPToSI, get_d(), get_i(32))
X_TO_Y_EMIT_ROUND(F64toI64S, CreateFPToSI, get_d(), get_i(64))

UNOP_EMIT(Not1, CreateNot)
UNOP_EMIT(Not8, CreateNot)
UNOP_EMIT(Not16, CreateNot)
UNOP_EMIT(Not32, CreateNot)
UNOP_EMIT(Not64, CreateNot)

X_TO_Y_EMIT(ReinterpF64asI64, CreateBitCast, get_d(), get_i(64))
X_TO_Y_EMIT(ReinterpI64asF64, CreateBitCast, get_i(64), get_d())
X_TO_Y_EMIT(ReinterpF32asI32, CreateBitCast, get_f(), get_i(32))
X_TO_Y_EMIT(ReinterpI32asF32, CreateBitCast, get_i(32), get_f())

Value* VexExprUnopV128to64::emit(void) const
{
	Value		*v_hilo;
	Constant	*shuffle_v[] = {
		get_32i(0), get_32i(1),
		get_32i(2), get_32i(3),
		get_32i(4), get_32i(5),
		get_32i(6), get_32i(7),
	};
	Constant	*cv;

	UNOP_SETUP
	v1 = builder->CreateBitCast(v1, get_vt(16, 8));
	cv = get_cv(shuffle_v);
	v_hilo = builder->CreateShuffleVector(v1, v1, cv, "v128extractlow_shuffle");

	return builder->CreateBitCast(
		v_hilo,
		get_i(64), 
		"V128to64");
}
Value* VexExprUnopV128HIto64::emit(void) const
{
	UNOP_SETUP
	return builder->CreateExtractElement(
		builder->CreateBitCast(v1, get_vt(2, 64)), 
		get_32i(1));
}

Value* VexExprBinop64HLto128::emit(void) const
{
	Value	*v_hi, *v_lo;

	BINOP_SETUP

	v_hi = builder->CreateZExt(v1, get_i(128));
	v_lo =  builder->CreateZExt(v2, get_i(128));

	// 2.6 expects same width ops. yuck.
	return builder->CreateOr(
		v_lo, 
		builder->CreateShl(v_hi, get_c(128, 64)));
}

Value* VexExprUnop128HIto64::emit(void) const
{
	UNOP_SETUP
	assert(v1->getType()->getPrimitiveSizeInBits() == 128);
	return builder->CreateExtractElement(
		builder->CreateBitCast(v1, get_vt(2, 64)), 
		get_32i(1));
}

/* so stupid */
Value* VexExprUnop32UtoV128::emit(void) const
{
	Value		*v_128i;

	UNOP_SETUP

	v_128i = builder->CreateZExt(v1, get_i(128));
	return builder->CreateBitCast(v_128i, get_vt(16, 8), "32UtoV128");
}

Value* VexExprUnop64UtoV128::emit(void) const
{
	Value		*v_128i;

	UNOP_SETUP

	v_128i = builder->CreateZExt(v1, get_i(128));
	
	return builder->CreateBitCast(v_128i, get_vt(16, 8), "64UtoV128");
}


Value* VexExprBinop64HLtoV128::emit(void) const
{
	Value		*v_hi, *v_lo, *v_128hl;

	BINOP_SETUP
	v1 = builder->CreateBitCast(v1, get_i(64));
	v2 = builder->CreateBitCast(v2, get_i(64));
	v_hi = builder->CreateZExt(v1, get_i(128));
	v_lo = builder->CreateZExt(v2, get_i(128));
	
	v_128hl = builder->CreateOr(
		v_lo,
		builder->CreateShl(v_hi, get_c(128, 64)));

	return builder->CreateBitCast(v_128hl, get_vt(16, 8), "64HLtoV128");
}

/* (i32, i32) -> i64 */
Value* VexExprBinop32HLto64::emit(void) const
{
	Value		*v_hi, *v_lo;

	BINOP_SETUP

	v_hi = builder->CreateZExt(v1, get_i(64));
	v_lo = builder->CreateZExt(v2, get_i(64));

	return builder->CreateOr(
		v_lo,
		builder->CreateShl(v_hi, get_c(64, 32)));
}

Value* VexExprBinop16HLto32::emit(void) const
{
	Value		*v_hi, *v_lo;

	BINOP_SETUP

	v_hi = builder->CreateZExt(v1, get_i(32));
	v_lo = builder->CreateZExt(v2, get_i(32));

	return builder->CreateOr(
		v_lo,
		builder->CreateShl(v_hi, get_c(32, 16)));
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

BINOP_EXPAND_EMIT(MullS8, SExt, get_i(16), Mul)
BINOP_EXPAND_EMIT(MullS16, SExt, get_i(32), Mul)
BINOP_EXPAND_EMIT(MullS32, SExt, get_i(64), Mul)
BINOP_EXPAND_EMIT(MullS64, SExt, get_i(128),Mul)

BINOP_EXPAND_EMIT(MullU8, ZExt, get_i(16), Mul)
BINOP_EXPAND_EMIT(MullU16, ZExt, get_i(32), Mul)
BINOP_EXPAND_EMIT(MullU32, ZExt, get_i(64), Mul)
BINOP_EXPAND_EMIT(MullU64, ZExt, get_i(128), Mul)


#define BINCMP_EMIT(x,y,z)					\
Value* VexExprBinop##x::emit(void) const			\
{								\
	BINOP_SETUP						\
	v1 = builder->CreateBitCast(v1, y);			\
	v2 = builder->CreateBitCast(v2, y);			\
	Value* result = builder->CreateBitCast(			\
		get_c(y->getBitWidth(), 0), y);			\
	for(unsigned i = 0; i < y->getNumElements(); ++i) {	\
		Value* e1 = builder->CreateExtractElement(	\
			v1, get_32i(i));			\
		Value* e2 = builder->CreateExtractElement(	\
			v2, get_32i(i));			\
		Value* elem = builder->CreateSelect(		\
			builder->Create##z(e1, e2),		\
			e1, e2);				\
		result = builder->CreateInsertElement(		\
			result, elem, get_32i(i));		\
	}							\
	return result;						\
}

BINCMP_EMIT(Max8Ux8 , get_vt(8 , 8 ), ICmpUGT);
BINCMP_EMIT(Max8Ux16, get_vt(16, 8 ), ICmpUGT);
BINCMP_EMIT(Max16Sx4, get_vt(4 , 16), ICmpSGT);
BINCMP_EMIT(Max16Sx8, get_vt(8 , 16), ICmpSGT);

BINCMP_EMIT(Min8Ux8 , get_vt(8 , 8 ), ICmpULT);
BINCMP_EMIT(Min8Ux16, get_vt(16, 8 ), ICmpULT);
BINCMP_EMIT(Min16Sx4, get_vt(4 , 16), ICmpSLT);
BINCMP_EMIT(Min16Sx8, get_vt(8 , 16), ICmpSLT);

BINCMP_EMIT(Min32Fx2, get_vtf(2), FCmpULT);
BINCMP_EMIT(Max32Fx2, get_vtf(2), FCmpUGT);
BINCMP_EMIT(Min32Fx4, get_vtf(4), FCmpULT);
BINCMP_EMIT(Max32Fx4, get_vtf(4), FCmpUGT);

BINCMP_EMIT(Max64Fx2, get_vtd(2), FCmpUGT);
BINCMP_EMIT(Min64Fx2, get_vtd(2), FCmpULT);

#define DBL_1 ConstantFP::get(get_d(), 1.0)
#define FLT_1 ConstantFP::get(get_f(), 1.0)

Constant* const g_dbl_1x2[] = { DBL_1, DBL_1 };
Constant* const g_flt_1x4[] = { FLT_1, FLT_1, FLT_1, FLT_1 };
#define DBL_1x2 get_cv(g_dbl_1x2)
#define FLT_1x4 get_cv(g_flt_1x4)

#define VINT(y,z)							\
	Function	*f;						\
	std::vector<const Type*> call_args;				\
	call_args.push_back(y);						\
	f = Intrinsic::getDeclaration(theGenLLVM->getModule(), 		\
		Intrinsic::z, &call_args[0], 1);			\
	assert (f != NULL);						\
	v1 = builder->CreateCall(f, v1);				

#define VRCP(o) v1 = builder->CreateFDiv(o, v1);

#define NONE

#define OPV_RSQ(x, y, z, a, b)						\
Value* VexExprUnop##x::emit(void) const					\
{									\
	UNOP_SETUP							\
	v1 = builder->CreateBitCast(v1, y);				\
	a								\
	b								\
	return v1;							\
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


BINOP_EMIT(Or8, Or)
BINOP_EMIT(Or16, Or)
BINOP_EMIT(Or32, Or)
BINOP_EMIT(Or64, Or)

#define SHIFT_EMIT(x,y,b)				\
Value* VexExprBinop##x::emit(void) const	\
{						\
	BINOP_SETUP				\
	return builder->Create##y(v1,		\
		builder->CreateZExt(	\
			builder->CreateTrunc(v2, get_i(b)),	\
			v1->getType()));	\
}
SHIFT_EMIT(Shl8, Shl, 3)
SHIFT_EMIT(Shl16, Shl, 4)
SHIFT_EMIT(Shl32, Shl, 5)
SHIFT_EMIT(Shl64, Shl, 6)
              
SHIFT_EMIT(Shr8, LShr, 3)
SHIFT_EMIT(Shr16, LShr, 4)
SHIFT_EMIT(Shr32, LShr, 5)
SHIFT_EMIT(Shr64, LShr, 6)
              
SHIFT_EMIT(Sar8, AShr, 3)
SHIFT_EMIT(Sar16, AShr, 4)
SHIFT_EMIT(Sar32, AShr, 5)
SHIFT_EMIT(Sar64, AShr, 6)

BINOP_EMIT(Sub8, Sub)
BINOP_EMIT(Sub16, Sub)
BINOP_EMIT(Sub32, Sub)
BINOP_EMIT(Sub64, Sub)

BINOP_EMIT(Xor8, Xor)
BINOP_EMIT(Xor16, Xor)
BINOP_EMIT(Xor32, Xor)
BINOP_EMIT(Xor64, Xor)

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

#define DIVMOD_EMIT(x,y,z,w)						\
Value* VexExprBinop##x::emit(void) const				\
{									\
	Value	*div, *rem;						\
	BINOP_SETUP							\
	v1 = builder->CreateBitCast(v1, get_i(w));			\
	v2 = builder->Create##z##Ext(v2, get_i(w)); 			\
	div = builder->Create##z##Ext(					\
		builder->Create##y##Div(v1, v2),			\
		get_i(w));						\
	rem = builder->Create##z##Ext(					\
		builder->Create##y##Rem(v1, v2),			\
		get_i(w));						\
	Constant	*shuffle_v[] = { get_32i(0), get_32i(2) };	\
	Constant	*cv = get_cv(shuffle_v);			\
	return builder->CreateBitCast( 					\
		builder->CreateShuffleVector(				\
			builder->CreateBitCast(div, get_vt(2, w / 2)),	\
			builder->CreateBitCast(rem, get_vt(2, w / 2)),	\
			cv),						\
		get_i(w));						\
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
	lo_op_lhs = builder->CreateExtractElement(v1, get_32i(0));	\
	lo_op_rhs = builder->CreateExtractElement(v2, get_32i(0));	\
	result = builder->Create##z(lo_op_lhs, lo_op_rhs);		\
	return builder->CreateInsertElement(v1, result, get_32i(0));	\
}

OPF0X_EMIT(Mul64F0x2, get_vtd(2), FMul)
OPF0X_EMIT(Div64F0x2, get_vtd(2), FDiv)
OPF0X_EMIT(Add64F0x2, get_vtd(2), FAdd)
OPF0X_EMIT(Sub64F0x2, get_vtd(2), FSub)
OPF0X_EMIT(Mul32F0x4, get_vtf(4), FMul)
OPF0X_EMIT(Div32F0x4, get_vtf(4), FDiv)
OPF0X_EMIT(Add32F0x4, get_vtf(4), FAdd)
OPF0X_EMIT(Sub32F0x4, get_vtf(4), FSub)

#define OPF0X_CMP_EMIT(x, y, z)						\
Value* VexExprBinop##x::emit(void) const				\
{									\
	Value	*a1, *a2, *result;					\
	BINOP_SETUP							\
	v1 = builder->CreateBitCast(v1, y);				\
	v2 = builder->CreateBitCast(v2, y);				\
	a1 = builder->CreateExtractElement(v1, get_32i(0));		\
	a2 = builder->CreateExtractElement(v2, get_32i(0));		\
	result = builder->Create##z(a1, a2);				\
	result = builder->CreateSExt(result, 				\
		get_i(y->getScalarType()->getPrimitiveSizeInBits()));	\
	result = builder->CreateBitCast(result, y->getScalarType());	\
	return builder->CreateInsertElement(v1, result, get_32i(0));	\
}

OPF0X_CMP_EMIT(CmpLT32F0x4, get_vtf(4), FCmpOLT);
OPF0X_CMP_EMIT(CmpLE32F0x4, get_vtf(4), FCmpOLT);
OPF0X_CMP_EMIT(CmpEQ32F0x4, get_vtf(4), FCmpOEQ);
OPF0X_CMP_EMIT(CmpUN32F0x4, get_vtf(4), FCmpUNO);

OPF0X_CMP_EMIT(CmpLT64F0x2, get_vtd(2), FCmpOLT);
OPF0X_CMP_EMIT(CmpLE64F0x2, get_vtd(2), FCmpOLE);
OPF0X_CMP_EMIT(CmpEQ64F0x2, get_vtd(2), FCmpOEQ);
OPF0X_CMP_EMIT(CmpUN64F0x2, get_vtd(2), FCmpUNO);

#define OPF0X_SEL_EMIT(x, y, z)						\
Value* VexExprBinop##x::emit(void) const				\
{									\
	Value	*result, *a, *b;					\
	BINOP_SETUP							\
	v1 = builder->CreateBitCast(v1, y);				\
	v2 = builder->CreateBitCast(v2, y);				\
	a = builder->CreateExtractElement(v1, get_32i(0));		\
	b = builder->CreateExtractElement(v2, get_32i(0));		\
	result = builder->CreateSelect(					\
		builder->Create##z(a, b), a, b);			\
	return builder->CreateInsertElement(v1, result, get_32i(0));	\
}

OPF0X_SEL_EMIT(Max32F0x4, get_vtf(4), FCmpUGT)
OPF0X_SEL_EMIT(Min32F0x4, get_vtf(4), FCmpULT)
OPF0X_SEL_EMIT(Max64F0x2, get_vtd(2), FCmpUGT)
OPF0X_SEL_EMIT(Min64F0x2, get_vtd(2), FCmpULT)

#define F0XINT(y,z)							\
	Function	*f;						\
	std::vector<const Type*> call_args;				\
	call_args.push_back(y->getScalarType());			\
	f = Intrinsic::getDeclaration(theGenLLVM->getModule(), 		\
		Intrinsic::z, &call_args[0], 1);			\
	assert (f != NULL);						\
	a1 = builder->CreateCall(f, a1);				

#define F0XRCP(o) a1 = builder->CreateFDiv(o, a1);

#define OPF0X_RSQ(x, y, z, a, b)					\
Value* VexExprUnop##x::emit(void) const					\
{									\
	Value		*a1;						\
	UNOP_SETUP							\
	v1 = builder->CreateBitCast(v1, y);				\
	a1 = builder->CreateExtractElement(v1, get_32i(0));		\
	a								\
	b								\
	return builder->CreateInsertElement(v1, a1, get_32i(0));	\
}

OPF0X_RSQ(Sqrt64F0x2 , get_vtd(2), sqrt, NONE, F0XINT(get_d(), sqrt))
OPF0X_RSQ(Recip64F0x2, get_vtd(2), sqrt, NONE, F0XRCP(DBL_1))
OPF0X_RSQ(RSqrt64F0x2, get_vtd(2), sqrt, F0XINT(get_d(), sqrt), F0XRCP(DBL_1))

OPF0X_RSQ(Sqrt32F0x4 , get_vtf(4), sqrt, NONE, F0XINT(get_f(), sqrt))
OPF0X_RSQ(Recip32F0x4, get_vtf(4), sqrt, NONE, F0XRCP(FLT_1))
OPF0X_RSQ(RSqrt32F0x4, get_vtf(4), sqrt, F0XINT(get_f(), sqrt), F0XRCP(FLT_1))

Value* VexExprBinopSetV128lo64::emit(void) const
{
	BINOP_SETUP
	return builder->CreateInsertElement(
		builder->CreateBitCast(v1, get_vt(2, 64)),
		v2,
		get_32i(0));
}

Value* VexExprBinopSetV128lo32::emit(void) const
{
	BINOP_SETUP
	return builder->CreateInsertElement(
		builder->CreateBitCast(v1, get_vt(4, 32)),
		v2,
		get_32i(0));
}

#define EMIT_HELPER_BINOP(x,y,z)		\
Value* VexExprBinop##x::emit(void) const	\
{						\
	llvm::Function	*f;			\
	BINOP_SETUP				\
	f = theVexHelpers->getHelper(y);	\
	assert (f != NULL);			\
	v1 = builder->CreateBitCast(v1, z);	\
	v2 = builder->CreateBitCast(v2, z);	\
	return builder->CreateCall2(f, v1, v2);	\
}

EMIT_HELPER_BINOP(CmpF64, "vexop_cmpf64", get_d())

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

static Constant* ileave_lo8x16[] =  {
	/* this was backwards (0,16) instead of (16,0)-- thx xchk! */
	get_32i(16), get_32i(0),
	get_32i(17), get_32i(1),
	get_32i(18), get_32i(2),
	get_32i(19), get_32i(3),
	get_32i(20), get_32i(4),
	get_32i(21), get_32i(5),
	get_32i(22), get_32i(6),
	get_32i(23), get_32i(7)};
static Constant* ileave_hi8x16[] =  {
	get_32i(24), get_32i(8),
	get_32i(25), get_32i(9),
	get_32i(26), get_32i(10),
	get_32i(27), get_32i(11),
	get_32i(28), get_32i(12),
	get_32i(29), get_32i(13),
	get_32i(30), get_32i(14),
	get_32i(31), get_32i(15)};
#define ileave_lo8x8 ileave_lo16x8
#define ileave_hi8x8 ileave_hi16x8
static Constant* ileave_lo16x8[] = { 
	get_32i(8),  get_32i(0),
	get_32i(9),  get_32i(1),
	get_32i(10), get_32i(2),
	get_32i(11), get_32i(3)};
static Constant* ileave_hi16x8[] = { 
	get_32i(12), get_32i(4),
	get_32i(13), get_32i(5),
	get_32i(14), get_32i(6),
	get_32i(15), get_32i(7)};
#define ileave_lo16x4 ileave_lo32x4
#define ileave_hi16x4 ileave_hi32x4
static Constant* ileave_lo32x4[] = { 
	get_32i(4),  get_32i(0),
	get_32i(5),  get_32i(1)};
static Constant* ileave_hi32x4[] = { 
	get_32i(6),  get_32i(2),
	get_32i(7),  get_32i(3)};
#define ileave_lo32x2 ileave_lo64x2
#define ileave_hi32x2 ileave_hi64x2
static Constant* ileave_lo64x2[] = { 
	get_32i(2),  get_32i(0) };
static Constant* ileave_hi64x2[] = {
	get_32i(3),  get_32i(1)};

#define EMIT_ILEAVE(x, y, w)						\
Value* VexExprBinopInterleave##x::emit(void) const			\
{									\
	Constant	*cv;						\
									\
	BINOP_SETUP							\
									\
	v1 = builder->CreateBitCast(v1, w, #x "_v1");			\
	v2 = builder->CreateBitCast(v2, w, #x "_v2");			\
									\
	cv = get_cv(y);							\
									\
	return builder->CreateShuffleVector(v1, v2, cv, "ileave_"#x);	\
}

EMIT_ILEAVE(LO8x8 , ileave_lo8x8 , get_vt(8 , 8 ))
EMIT_ILEAVE(HI8x8 , ileave_hi8x8 , get_vt(8 , 8 ))
EMIT_ILEAVE(LO8x16, ileave_lo8x16, get_vt(16, 8 ))
EMIT_ILEAVE(HI8x16, ileave_hi8x16, get_vt(16, 8 ))
EMIT_ILEAVE(LO16x4, ileave_lo16x4, get_vt(4 , 16))
EMIT_ILEAVE(HI16x4, ileave_hi16x4, get_vt(4 , 16))
EMIT_ILEAVE(LO16x8, ileave_lo16x8, get_vt(8 , 16))
EMIT_ILEAVE(HI16x8, ileave_hi16x8, get_vt(8 , 16))
EMIT_ILEAVE(LO32x2, ileave_lo32x2, get_vt(2 , 32))
EMIT_ILEAVE(HI32x2, ileave_hi32x2, get_vt(2 , 32))
EMIT_ILEAVE(LO32x4, ileave_lo32x4, get_vt(4 , 32))
EMIT_ILEAVE(HI32x4, ileave_hi32x4, get_vt(4 , 32))
EMIT_ILEAVE(LO64x2, ileave_lo64x2, get_vt(2 , 64))
EMIT_ILEAVE(HI64x2, ileave_hi64x2, get_vt(2 , 64))

#define UNOPV_EMIT(x, y, z)			\
Value* VexExprUnop##x::emit(void) const	\
{	\
	UNOP_SETUP					\
	v1 = builder->CreateBitCast(v1, y);		\
	return builder->Create##z(v1);		\
}

#define OPV_EMIT(x, y, z)			\
Value* VexExprBinop##x::emit(void) const	\
{	\
	BINOP_SETUP					\
	v1 = builder->CreateBitCast(v1, y);		\
	v2 = builder->CreateBitCast(v2, y);		\
	return builder->Create##z(v1, v2);		\
}

OPV_EMIT(OrV128, get_i(128), Or)
OPV_EMIT(XorV128, get_i(128), Xor)
OPV_EMIT(AndV128, get_i(128), And)
UNOPV_EMIT(NotV128, get_i(128), Not)



OPV_EMIT(Shl8x8 , get_vt(8, 8), Shl )
OPV_EMIT(Shr8x8 , get_vt(8, 8), LShr)
OPV_EMIT(Sar8x8 , get_vt(8, 8), AShr)
OPV_EMIT(Sal8x8 , get_vt(8, 8), Shl )  //??

OPV_EMIT(Shl8x16 , get_vt(16, 8), Shl )
OPV_EMIT(Shr8x16 , get_vt(16, 8), LShr)
OPV_EMIT(Sar8x16 , get_vt(16, 8), AShr)
OPV_EMIT(Sal8x16 , get_vt(16, 8), Shl )  //??

OPV_EMIT(Shl16x4 , get_vt(4, 16), Shl )
OPV_EMIT(Shr16x4 , get_vt(4, 16), LShr)
OPV_EMIT(Sar16x4 , get_vt(4, 16), AShr)
OPV_EMIT(Sal16x4 , get_vt(4, 16), Shl )  //??

OPV_EMIT(Shl16x8 , get_vt(8, 16), Shl )
OPV_EMIT(Shr16x8 , get_vt(8, 16), LShr)
OPV_EMIT(Sar16x8 , get_vt(8, 16), AShr)
OPV_EMIT(Sal16x8 , get_vt(8, 16), Shl )  //??

OPV_EMIT(Shl32x2 , get_vt(2, 32), Shl )
OPV_EMIT(Shr32x2 , get_vt(2, 32), LShr)
OPV_EMIT(Sar32x2 , get_vt(2, 32), AShr)
OPV_EMIT(Sal32x2 , get_vt(2, 32), Shl )  //??

OPV_EMIT(Shl32x4 , get_vt(4, 32), Shl )
OPV_EMIT(Shr32x4 , get_vt(4, 32), LShr)
OPV_EMIT(Sar32x4 , get_vt(4, 32), AShr)
OPV_EMIT(Sal32x4 , get_vt(4, 32), Shl )  //??

OPV_EMIT(Shl64x2 , get_vt(2, 64), Shl )
OPV_EMIT(Shr64x2 , get_vt(2, 64), LShr)
OPV_EMIT(Sar64x2 , get_vt(2, 64), AShr)
OPV_EMIT(Sal64x2 , get_vt(2, 64), Shl )  //??

OPV_EMIT(Add8x8  , get_vt(8, 8) , Add )
OPV_EMIT(Add8x16 , get_vt(16, 8), Add )
OPV_EMIT(Add16x4 , get_vt(4, 16), Add )
OPV_EMIT(Add16x8 , get_vt(8, 16), Add )
OPV_EMIT(Add32x2 , get_vt(2, 32), Add )
OPV_EMIT(Add32x4 , get_vt(4, 32), Add )
OPV_EMIT(Add64x2 , get_vt(2, 64), Add )

OPV_EMIT(Sub8x8  , get_vt(8, 8) , Sub )
OPV_EMIT(Sub8x16 , get_vt(16, 8), Sub )
OPV_EMIT(Sub16x4 , get_vt(4, 16), Sub )
OPV_EMIT(Sub16x8 , get_vt(8, 16), Sub )
OPV_EMIT(Sub32x2 , get_vt(2, 32), Sub )
OPV_EMIT(Sub32x4 , get_vt(4, 32), Sub )
OPV_EMIT(Sub64x2 , get_vt(2, 64), Sub )

OPV_EMIT(Mul8x8  , get_vt(8, 8) , Mul )
OPV_EMIT(Mul8x16 , get_vt(16, 8), Mul )
OPV_EMIT(Mul16x4 , get_vt(4, 16), Mul )
OPV_EMIT(Mul16x8 , get_vt(8, 16), Mul )
OPV_EMIT(Mul32x2 , get_vt(2, 32), Mul )
OPV_EMIT(Mul32x4 , get_vt(4, 32), Mul )


// name, dstTy, ext type, intermediate type, shift bits, op
#define OPV_EXT_EMIT(x, y, z, w, s, o)				\
Value* VexExprBinop##x::emit(void) const			\
{								\
	BINOP_SETUP						\
	v1 = builder->CreateBitCast(v1, y);			\
	v1 = builder->Create##z(v1, w);				\
	v2 = builder->CreateBitCast(v2, y);			\
	v2 = builder->Create##z(v2, w);				\
	Value* full = builder->Create##o(v1, v2);		\
	Value *shift = get_c(s * 2, s);				\
	assert(y->getNumElements() <= 8);			\
	shift = builder->CreateZExt(shift,			\
		get_i(w->getBitWidth()));			\
	shift = builder->CreateBitCast(shift, w);		\
	Constant *shuffle_v[] = {   				\
		get_32i(0), get_32i(0),				\
		get_32i(0), get_32i(0),				\
		get_32i(0), get_32i(0),				\
		get_32i(0), get_32i(0),				\
	};							\
	Constant *cv = get_cvl(shuffle_v, 			\
		y->getNumElements());				\
	shift = builder->CreateShuffleVector(shift, shift, cv);	\
	return builder->CreateTrunc(				\
		builder->CreateLShr(full, shift), y);		\
}

OPV_EXT_EMIT(MulHi16Ux4, get_vt(4, 16), ZExt, get_vt(4, 32), 16, Mul)
OPV_EXT_EMIT(MulHi16Sx4, get_vt(4, 16), SExt, get_vt(4, 32), 16, Mul)
OPV_EXT_EMIT(MulHi16Ux8, get_vt(8, 16), ZExt, get_vt(8, 32), 16, Mul)
OPV_EXT_EMIT(MulHi16Sx8, get_vt(8, 16), SExt, get_vt(8, 32), 16, Mul)
OPV_EXT_EMIT(MulHi32Ux4, get_vt(4, 32), ZExt, get_vt(4, 64), 32, Mul)
OPV_EXT_EMIT(MulHi32Sx4, get_vt(4, 32), SExt, get_vt(4, 64), 32, Mul)

// name, destTy
#define OPNRW_EMIT(x, w)					\
Value* VexExprBinop##x::emit(void) const			\
{								\
	BINOP_SETUP						\
	v1 = builder->CreateBitCast(v1, w);			\
	v2 = builder->CreateBitCast(v2, w);			\
	assert(w->getNumElements() <= 16);			\
	assert(false && "Narrow is untested...test me :-)!");	\
	Constant *shuffle_v[] = {   				\
		get_32i(0), get_32i(2),				\
		get_32i(4), get_32i(6),				\
		get_32i(8), get_32i(10),			\
		get_32i(12), get_32i(14),			\
		get_32i(16), get_32i(18),			\
		get_32i(20), get_32i(22),			\
		get_32i(24), get_32i(26),			\
		get_32i(28), get_32i(30),			\
	};							\
	Constant *cv = get_cvl(shuffle_v, 			\
		w->getNumElements());				\
	return builder->CreateShuffleVector(v1, v2, cv);	\
}

OPNRW_EMIT(Narrow16x8, get_vt(16, 8));
OPNRW_EMIT(Narrow32x4, get_vt(8, 16));

//LLVM is busted and doesnt work properly with a vector SELECT opcode...

// name, srcTy, destTy, compare const, 
#define OPNRWSS_EMIT(x, y, w, c, neg_c)				\
Value* VexExprBinop##x::emit(void) const			\
{								\
	BINOP_SETUP						\
	v1 = builder->CreateBitCast(v1, y);			\
	v2 = builder->CreateBitCast(v2, y);			\
	Value* result = builder->CreateBitCast(			\
		get_c(w->getBitWidth(), 0), w);			\
	for(unsigned i = 0; i < y->getNumElements(); ++i) {	\
		Value* elem = builder->CreateExtractElement(	\
			v2, get_32i(i));			\
		elem = builder->CreateSelect(			\
			builder->CreateICmpSGT(elem, c),	\
			c, elem);				\
		elem = builder->CreateSelect(			\
			builder->CreateICmpSLT(elem, neg_c),	\
			neg_c, elem);				\
		elem = builder->CreateTrunc(elem, 		\
			w->getScalarType());			\
		result = builder->CreateInsertElement(		\
			result, elem, get_32i(i));		\
	}							\
	for(unsigned i = 0; i < y->getNumElements(); ++i) {	\
		Value* elem = builder->CreateExtractElement(	\
			v1, get_32i(i));			\
		elem = builder->CreateSelect(			\
			builder->CreateICmpSGT(elem, c),	\
			c, elem);				\
		elem = builder->CreateSelect(			\
			builder->CreateICmpSLT(elem, neg_c),	\
			neg_c, elem);				\
		elem = builder->CreateTrunc(elem, 		\
			w->getScalarType());			\
		result = builder->CreateInsertElement(		\
			result, elem, 				\
			get_32i(i + y->getNumElements()));	\
	}							\
	return result;						\
}

OPNRWSS_EMIT(QNarrow16Ux4, get_vt(4, 16), get_vt(8 , 8 ), get_c(16, 0x00FF), get_c(16, 0));
OPNRWSS_EMIT(QNarrow16Ux8, get_vt(8, 16), get_vt(16, 8 ), get_c(16, 0x00FF), get_c(16, 0));
OPNRWSS_EMIT(QNarrow32Ux4, get_vt(4, 32), get_vt(8 , 16), get_c(32, 0xFFFF), get_c(32, 0));

OPNRWSS_EMIT(QNarrow16Sx4, get_vt(4, 16), get_vt(8 , 8 ), get_c(16, 0x7F)  , get_c(16, 0xFF80    ));
OPNRWSS_EMIT(QNarrow16Sx8, get_vt(8, 16), get_vt(16, 8 ), get_c(16, 0x7F)  , get_c(16, 0xFF80    ));
OPNRWSS_EMIT(QNarrow32Sx2, get_vt(2, 32), get_vt(4 , 16), get_c(32, 0x7FFF), get_c(32, 0xFFFF8000));
OPNRWSS_EMIT(QNarrow32Sx4, get_vt(4, 32), get_vt(8 , 16), get_c(32, 0x7FFF), get_c(32, 0xFFFF8000));


#define OPV_CMP_T_EMIT(x, y, z, w)			\
Value* VexExprBinop##x::emit(void) const		\
{							\
	BINOP_SETUP					\
	v1 = builder->CreateBitCast(v1, y);		\
	v2 = builder->CreateBitCast(v2, y);		\
	return builder->CreateSExt(			\
		builder->Create##z(v1, v2),		\
		w);					\
}

#define OPV_CMP_EMIT(x, y, z)	OPV_CMP_T_EMIT(x, y, z, y)

OPV_CMP_EMIT(CmpEQ8x8 , get_vt(8, 8) , ICmpEQ)
OPV_CMP_EMIT(CmpEQ8x16, get_vt(16, 8), ICmpEQ)
OPV_CMP_EMIT(CmpEQ16x4, get_vt(4, 16), ICmpEQ)
OPV_CMP_EMIT(CmpEQ16x8, get_vt(8, 16), ICmpEQ)
OPV_CMP_EMIT(CmpEQ32x2, get_vt(2, 32), ICmpEQ)
OPV_CMP_EMIT(CmpEQ32x4, get_vt(4, 32), ICmpEQ)

OPV_CMP_EMIT(CmpGT8Sx8 , get_vt(8, 8) , ICmpSGT)
OPV_CMP_EMIT(CmpGT8Sx16, get_vt(16, 8), ICmpSGT)
OPV_CMP_EMIT(CmpGT16Sx4, get_vt(4, 16), ICmpSGT)
OPV_CMP_EMIT(CmpGT16Sx8, get_vt(8, 16), ICmpSGT)
OPV_CMP_EMIT(CmpGT32Sx2, get_vt(2, 32), ICmpSGT)
OPV_CMP_EMIT(CmpGT32Sx4, get_vt(4, 32), ICmpSGT)
OPV_CMP_EMIT(CmpGT64Sx2, get_vt(2, 64), ICmpSGT)

OPV_CMP_EMIT(CmpGT8Ux8 , get_vt(8, 8) , ICmpUGT)
OPV_CMP_EMIT(CmpGT8Ux16, get_vt(16, 8), ICmpUGT)
OPV_CMP_EMIT(CmpGT16Ux4, get_vt(4, 16), ICmpUGT)
OPV_CMP_EMIT(CmpGT16Ux8, get_vt(8, 16), ICmpUGT)
OPV_CMP_EMIT(CmpGT32Ux2, get_vt(2, 32), ICmpUGT)
OPV_CMP_EMIT(CmpGT32Ux4, get_vt(4, 32), ICmpUGT)

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

#define DBL_0 ConstantFP::get(get_d(), 0.0)
#define FLT_0 ConstantFP::get(get_f(), 0.0)

// Constant* const g_dbl_0x2[] = { DBL_0, DBL_0 };
Constant* const g_flt_0x2[] = { FLT_0, FLT_0 };
Constant* const g_flt_0x4[] = { FLT_0, FLT_0, FLT_0, FLT_0 };

#define UNOP_NEG_EMIT(x, y, c)  \
Value* VexExprUnop##x::emit(void) const		\
{						\
	UNOP_SETUP				\
	v1 = builder->CreateBitCast(v1, y);	\
	return builder->CreateFSub(c, v1);	\
}

UNOP_NEG_EMIT(NegF32, get_f(), FLT_0)
UNOP_NEG_EMIT(NegF64, get_d(), DBL_0)
UNOP_NEG_EMIT(Neg32Fx2, get_vtf(2), get_cv(g_flt_0x2))
UNOP_NEG_EMIT(Neg32Fx4, get_vtf(4), get_cv(g_flt_0x4))


#define TRIOP_EXPAND_EMIT(x,y,z,w)				\
Value* VexExprTriop##x::emit(void) const			\
{								\
	TRIOP_SETUP						\
	v2 = builder->Create##y(v2, z);				\
	v3 = builder->Create##y(v3, z);				\
	return builder->Create##w(v2, v3);			\
}

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
	return builder->CreateSIToFP(
		builder->CreateFPToSI(v2, get_i(32)), 
		get_f());
}

Value* VexExprBinopRoundF64toInt::emit(void) const
{
	/* TODO: rounding mode */
	BINOP_SETUP
	v2 = builder->CreateBitCast(v2, get_d());
	return builder->CreateSIToFP(
		builder->CreateFPToSI(v2, get_i(64)), 
		get_d());
}


//note max vector elements of 16
#define OPVS_EMIT(x, y, z)				\
Value* VexExprBinop##x::emit(void) const		\
{							\
	BINOP_SETUP					\
	assert(y->getNumElements() <= 16);		\
	v1 = builder->CreateBitCast(v1, y);		\
	v2 = builder->CreateZExt(v2, 			\
		get_i(y->getBitWidth()));		\
	v2 = builder->CreateBitCast(v2, y);		\
	Constant *shuffle_v[] = {   			\
		get_32i(0), get_32i(0),			\
		get_32i(0), get_32i(0),			\
		get_32i(0), get_32i(0),			\
		get_32i(0), get_32i(0),			\
		get_32i(0), get_32i(0),			\
		get_32i(0), get_32i(0),			\
		get_32i(0), get_32i(0),			\
		get_32i(0), get_32i(0),			\
	};						\
	Constant *cv = get_cvl(shuffle_v, 		\
		y->getNumElements());			\
	v2 = builder->CreateShuffleVector(v2, v2, cv);	\
	return builder->Create##z(v1, v2);		\
}

OPVS_EMIT(ShlN8x8, get_vt(8, 8), Shl )
OPVS_EMIT(SarN8x8, get_vt(8, 8), AShr)
OPVS_EMIT(ShrN8x8, get_vt(8, 8), LShr)

OPVS_EMIT(ShlN8x16, get_vt(16, 8), Shl )
OPVS_EMIT(SarN8x16, get_vt(16, 8), AShr)
OPVS_EMIT(ShrN8x16, get_vt(16, 8), LShr)

OPVS_EMIT(ShlN16x4, get_vt(4, 16), Shl)
OPVS_EMIT(SarN16x4, get_vt(4, 16), AShr)
OPVS_EMIT(ShrN16x4, get_vt(4, 16), LShr )

OPVS_EMIT(ShlN16x8, get_vt(8, 16), Shl)
OPVS_EMIT(SarN16x8, get_vt(8, 16), AShr)
OPVS_EMIT(ShrN16x8, get_vt(8, 16), LShr )

OPVS_EMIT(ShlN32x2, get_vt(2, 32), Shl)
OPVS_EMIT(SarN32x2, get_vt(2, 32), AShr)
OPVS_EMIT(ShrN32x2, get_vt(2, 32), LShr )

OPVS_EMIT(ShlN32x4, get_vt(4, 32), Shl)
OPVS_EMIT(SarN32x4, get_vt(4, 32), AShr)
OPVS_EMIT(ShrN32x4, get_vt(4, 32), LShr )

OPVS_EMIT(ShlN64x2, get_vt(2, 64), Shl)
OPVS_EMIT(SarN64x2, get_vt(2, 64), AShr)
OPVS_EMIT(ShrN64x2, get_vt(2, 64), LShr )

/* sadly the shuffle opcode requires a constant vector which is not
   what this opcode always receives, TODO: check for constant vector
   and do the faster version with the shuffle opcode. */
#define OPSHUF_EMIT(x, y, z)						\
Value* VexExprBinop##x::emit(void) const				\
{									\
	BINOP_SETUP							\
	v1 = builder->CreateBitCast(v1, y);				\
	v2 = builder->CreateBitCast(v2, y);				\
	Value* result = get_c(y->getBitWidth(), 0);			\
	result = builder->CreateBitCast(result, y);			\
	for(unsigned i = 0; i < y->getNumElements(); ++i) {		\
		result = builder->CreateInsertElement(result,		\
			builder->CreateExtractElement(v1,		\
				builder->CreateZExt(			\
					builder->CreateExtractElement(	\
						v2, get_32i(i)),	\
						get_i(32))),		\
			get_32i(i));					\
	}								\
	return result;							\
}

OPSHUF_EMIT(Perm8x8, get_vt(8, 8), get_vt(8, 8))
OPSHUF_EMIT(Perm8x16, get_vt(16, 8), get_vt(16, 8))

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

