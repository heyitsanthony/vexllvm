#include <stdio.h>
#include <iostream>
#include "genllvm.h"
#include "vexop.h"
#include "vexop_macros.h"
#include "vexhelpers.h"
#include <llvm/IR/Intrinsics.h>
#include <llvm/Analysis/ConstantFolding.h>

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
CASE_OP(I64UtoF64)
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

CASE_OP(NarrowBin16to8x16);
CASE_OP(NarrowBin32to16x8);
CASE_OP(QNarrowBin16Sto8Sx8);
CASE_OP(QNarrowBin16Sto8Sx16);
CASE_OP(QNarrowBin32Sto16Sx4);
CASE_OP(QNarrowBin32Sto16Sx8);
CASE_OP(QNarrowUn16Sto8Ux8);
CASE_OP(QNarrowBin16Sto8Ux16);
CASE_OP(QNarrowBin32Sto16Ux8);

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

XHI_TO_Y_EMIT(16HIto8, 8, get_i(16), get_i(8))
XHI_TO_Y_EMIT(64HIto32, 32, get_i(64), get_i(32))
XHI_TO_Y_EMIT(32HIto16, 16, get_i(32), get_i(16))

X_TO_Y_EMIT(16to8,    CreateTrunc, get_i(16), get_i(8))
X_TO_Y_EMIT(64to32,   CreateTrunc, get_i(64), get_i(32))
X_TO_Y_EMIT(32Uto64,  CreateZExt,  get_i(32), get_i(64))
X_TO_Y_EMIT(32Sto64,  CreateSExt,  get_i(32), get_i(64))
X_TO_Y_EMIT(32to8,    CreateTrunc, get_i(32), get_i(8))
X_TO_Y_EMIT(32to16,   CreateTrunc, get_i(32), get_i(16))
X_TO_Y_EMIT(64to1,    CreateTrunc, get_i(64), get_i(1))
X_TO_Y_EMIT(32to1,    CreateTrunc, get_i(32), get_i(1))
X_TO_Y_EMIT(64to8,    CreateTrunc, get_i(64), get_i(8))
X_TO_Y_EMIT(64to16,   CreateTrunc, get_i(64), get_i(16))
X_TO_Y_EMIT(1Uto8,    CreateZExt,  get_i(1),  get_i(8))
X_TO_Y_EMIT(1Uto32,   CreateZExt,  get_i(1),  get_i(32))
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


UNOP_EMIT(Not1, CreateNot)
UNOP_EMIT(Not8, CreateNot)
UNOP_EMIT(Not16, CreateNot)
UNOP_EMIT(Not32, CreateNot)
UNOP_EMIT(Not64, CreateNot)

X_TO_Y_EMIT(ReinterpF64asI64, CreateBitCast, get_d(), get_i(64))
X_TO_Y_EMIT(ReinterpI64asF64, CreateBitCast, get_i(64), get_d())
X_TO_Y_EMIT(ReinterpF32asI32, CreateBitCast, get_f(), get_i(32))
X_TO_Y_EMIT(ReinterpI32asF32, CreateBitCast, get_i(32), get_f())

Value* VexExprUnopDup8x16::emit(void) const
{
	Value	*v[5];
	UNOP_SETUP


	v[0] = builder->CreateZExt(v1, get_i(8*16));
	/* [0] = 0000 00xx
	 * [1] = 0000 xxxx ([0] | [0] << 8)
	 * [2] = xxxx xxxx ([1] | [1] << 16)
	 * [3] = 64
	 * [4] = 128
	 */
	for (unsigned i = 1; i <= 4; i++) {
		v[i] = builder->CreateOr(
			v[i-1],
			builder->CreateShl(v[i-1], (8 << (i-1))));
	}

	return v[4];
}

/* GET / SET elements of VECTOR
   GET is binop (I64, I8) -> I<elem_size>
   SET is triop (I64, I8, I<elem_size>) -> I64 */
#define GET_ELEM_EMIT(w,e)	\
Value* VexExprBinopGetElem ##w## x ##e ::emit(void) const { \
	BINOP_SETUP	\
	v1 = builder->CreateBitCast(v1, get_vt(e, w));	\
	return builder->CreateExtractElement(v1, v2);	}

#define SET_ELEM_EMIT(w,e)	\
Value* VexExprTriopSetElem ##w## x ##e ::emit(void) const { \
	TRIOP_SETUP	\
	v1 = builder->CreateBitCast(v1, get_vt(e, w));	\
	return builder->CreateInsertElement(v1, v3, v2); }

SET_ELEM_EMIT(8,8)
SET_ELEM_EMIT(32,2)
GET_ELEM_EMIT(8,8)
GET_ELEM_EMIT(32,2)

Value* VexExprUnopV128to64::emit(void) const
{
	UNOP_SETUP
	return builder->CreateExtractElement(
		builder->CreateBitCast(v1, get_vt(2, 64)),
		get_32i(0));
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


BINCMP_EMIT(Max8Ux8 , get_vt(8 , 8 ), ICmpUGT);
BINCMP_EMIT(Max8Ux16, get_vt(16, 8 ), ICmpUGT);
BINCMP_EMIT(Max16Sx4, get_vt(4 , 16), ICmpSGT);
BINCMP_EMIT(Max16Sx8, get_vt(8 , 16), ICmpSGT);

BINCMP_EMIT(Min8Ux8 , get_vt(8 , 8 ), ICmpULT);
BINCMP_EMIT(Min8Ux16, get_vt(16, 8 ), ICmpULT);
BINCMP_EMIT(Min16Sx4, get_vt(4 , 16), ICmpSLT);
BINCMP_EMIT(Min16Sx8, get_vt(8 , 16), ICmpSLT);

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

/* I tried to do this with vector instructions and it either
 * 1) created constantexprs that wouldn't fold to a constant
 * 2) crashed the JIT on make tests
 */
#define DIVMOD_EMIT(x,y,z,w)				\
Value* VexExprBinop##x::emit(void) const		\
{							\
	Value	*div, *rem;				\
	BINOP_SETUP					\
	v1 = builder->CreateBitCast(v1, get_i(w));	\
	v2 = builder->Create##z##Ext(v2, get_i(w)); 	\
	div = builder->Create##y##Div(v1, v2);		\
	rem = builder->Create##y##Rem(v1, v2);		\
	rem = builder->CreateShl(rem, get_c(w, w/2));	\
	div = builder->CreateZExt(			\
		builder->CreateTrunc(div,get_i(w/2)), get_i(w)); \
	return builder->CreateOr(div, rem);		\
}

DIVMOD_EMIT(DivModU128to64, U, Z, 128)
DIVMOD_EMIT(DivModS128to64, S, S, 128)
DIVMOD_EMIT(DivModU64to32, U, Z, 64)
DIVMOD_EMIT(DivModS64to32, S, S, 64)

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

BINOP_EMIT(CmpLE64S, ICmpSLE)
BINOP_EMIT(CmpLE64U, ICmpULE)
BINOP_EMIT(CmpLT64S, ICmpSLT)
BINOP_EMIT(CmpLT64U, ICmpULT)
BINOP_EMIT(CmpLE32S, ICmpSLE)
BINOP_EMIT(CmpLE32U, ICmpULE)
BINOP_EMIT(CmpLT32S, ICmpSLT)
BINOP_EMIT(CmpLT32U, ICmpULT)

/* count number of zeros (from bit0) leading up to first 1. */
/* 0x0 -> undef */
/* 0x1 -> 1 */
/* 0x2 -> 2 */
/* 0x3 -> 1 */
/* 0x4 -> 3 */
EMIT_HELPER_UNOP(Ctz64, "vexop_ctz64")
EMIT_HELPER_UNOP(Clz64, "vexop_clz64")
EMIT_HELPER_UNOP(Ctz32, "vexop_ctz32")
EMIT_HELPER_UNOP(Clz32, "vexop_clz32")

EMIT_HELPER_UNOP(AbsF64, "vexop_absf64")
EMIT_HELPER_UNOP(AbsF32, "vexop_absf32")

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

OPNRW_EMIT(NarrowBin16to8x16, get_vt(16, 8));
OPNRW_EMIT(NarrowBin32to16x8, get_vt(8, 16));

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

OPNRWSS_EMIT(
	QNarrowUn16Sto8Ux8, get_vt(4, 16), get_vt(8 , 8 ),
	get_c(16, 0x00FF), get_c(16, 0));

OPNRWSS_EMIT(
	QNarrowBin16Sto8Ux16, get_vt(8, 16), get_vt(16, 8),
	get_c(16, 0x00FF), get_c(16, 0));

OPNRWSS_EMIT(
	QNarrowBin16Sto8Ux8, get_vt(4, 16), get_vt(8, 8),
	get_c(16, 0x00FF), get_c(16, 0));

OPNRWSS_EMIT(
	QNarrowBin32Sto16Ux8, get_vt(4, 32), get_vt(8 , 16),
	get_c(32, 0xFFFF), get_c(32, 0));

OPNRWSS_EMIT(
	QNarrowBin16Sto8Sx8, get_vt(4, 16), get_vt(8 , 8),
	get_c(16, 0x7F)  , get_c(16, 0xFF80));

OPNRWSS_EMIT(
	QNarrowBin16Sto8Sx16, get_vt(8, 16), get_vt(16, 8),
	get_c(16, 0x7F)  , get_c(16, 0xFF80));

OPNRWSS_EMIT(
	QNarrowBin32Sto16Sx4, get_vt(2, 32), get_vt(4 , 16),
	get_c(32, 0x7FFF), get_c(32, 0xFFFF8000));

OPNRWSS_EMIT(
	QNarrowBin32Sto16Sx8, get_vt(4, 32), get_vt(8 , 16),
	get_c(32, 0x7FFF), get_c(32, 0xFFFF8000));

#define SHIGH(w, c, x, e, v) 					\
	Value *hi = get_c(w, c);				\
	hi = builder->Create##x##Ext(hi, v->getScalarType());	\
	elem = builder->CreateSelect(				\
		builder->CreateICmp##e##GT(elem, hi),		\
		hi, elem);				

#define SLOW(w, c, x, e, v) 					\
	Value *lo = get_c(w,c);					\
	lo = builder->Create##x##Ext(lo, v->getScalarType());	\
	elem = builder->CreateSelect(				\
		builder->CreateICmp##e##LT(elem, lo),		\
		lo, elem);					


#define OPSAT_EMIT(x, y, ext, w, op, one, two)			\
Value* VexExprBinop##x::emit(void) const			\
{								\
	BINOP_SETUP						\
	v1 = builder->CreateBitCast(v1, y);			\
	v2 = builder->CreateBitCast(v2, y);			\
	v1 = builder->Create##ext(v1, w);			\
	v2 = builder->Create##ext(v2, w);			\
	Value* result = builder->CreateBitCast(			\
		get_c(y->getBitWidth(), 0), y);			\
	Value* partial = builder->Create##op(v1, v2);		\
	for(unsigned i = 0; i < y->getNumElements(); ++i) {	\
		Value* elem = builder->CreateExtractElement(	\
			partial, get_32i(i));			\
		one						\
		two						\
		elem = builder->CreateTrunc(elem, 		\
			y->getScalarType());			\
		result = builder->CreateInsertElement(		\
			result, elem, get_32i(i));		\
	}							\
	return result;						\
}

#define OPSATS_AS_EMIT(a, b, d, e, f, g)			\
	OPSAT_EMIT(QAdd##a, b, SExt, d, Add, 			\
		SHIGH(e, f, S, S, d), SLOW(e, g, S, S, d));	\
	OPSAT_EMIT(QSub##a, b, SExt, d, Sub, 			\
		SHIGH(e, f, S, S, d), SLOW(e, g, S, S, d));

OPSATS_AS_EMIT(64Sx1, get_vt(1, 64), get_vt(1, 65), 64, 0x7FFFFFFFFFFFFFFFULL, 0x8000000000000000ULL);
OPSATS_AS_EMIT(64Sx2, get_vt(2, 64), get_vt(2, 65), 64, 0x7FFFFFFFFFFFFFFFULL, 0x8000000000000000ULL);
OPSATS_AS_EMIT(32Sx2, get_vt(2, 32), get_vt(2, 33), 32, 0x7FFFFFFF, 0x80000000);
OPSATS_AS_EMIT(32Sx4, get_vt(4, 32), get_vt(4, 33), 32, 0x7FFFFFFF, 0x80000000);
OPSATS_AS_EMIT(16Sx2, get_vt(2, 16), get_vt(2, 17), 16, 0x7FFF, 0x8000);
OPSATS_AS_EMIT(16Sx4, get_vt(4, 16), get_vt(4, 17), 16, 0x7FFF, 0x8000);
OPSATS_AS_EMIT(16Sx8, get_vt(8, 16), get_vt(8, 17), 16, 0x7FFF, 0x8000);
OPSATS_AS_EMIT(8Sx4,  get_vt(4 , 8), get_vt(4 , 9), 8, 0x7F, 0x80);
OPSATS_AS_EMIT(8Sx8,  get_vt(8 , 8), get_vt(8 , 9), 8, 0x7F, 0x80);
OPSATS_AS_EMIT(8Sx16, get_vt(16, 8), get_vt(16, 9), 8, 0x7F, 0x80);

#define OPSATU_AS_EMIT(a, b, d, e, f, g)				 \
	OPSAT_EMIT(QAdd##a, b, ZExt, d, Add, SHIGH(e, f, Z, U, d), NONE);\
	OPSAT_EMIT(QSub##a, b, ZExt, d, Sub, NONE, SLOW(e, g, Z, S, d)); \

OPSATU_AS_EMIT(64Ux1, get_vt(1, 64), get_vt(1, 65), 64, 0xFFFFFFFFFFFFFFFFULL, 0);
OPSATU_AS_EMIT(64Ux2, get_vt(2, 64), get_vt(2, 65), 64, 0xFFFFFFFFFFFFFFFFULL, 0);
OPSATU_AS_EMIT(32Ux2, get_vt(2, 32), get_vt(2, 33), 32, 0xFFFFFFFF, 0);
OPSATU_AS_EMIT(32Ux4, get_vt(4, 32), get_vt(4, 33), 32, 0xFFFFFFFF, 0);
OPSATU_AS_EMIT(16Ux2, get_vt(2, 16), get_vt(2, 17), 16, 0xFFFF, 0);
OPSATU_AS_EMIT(16Ux4, get_vt(4, 16), get_vt(4, 17), 16, 0xFFFF, 0);
OPSATU_AS_EMIT(16Ux8, get_vt(8, 16), get_vt(8, 17), 16, 0xFFFF, 0);
OPSATU_AS_EMIT(8Ux4,  get_vt(4 , 8), get_vt(4 , 9), 8, 0xFF, 0);
OPSATU_AS_EMIT(8Ux8,  get_vt(8 , 8), get_vt(8 , 9), 8, 0xFF, 0);
OPSATU_AS_EMIT(8Ux16, get_vt(16, 8), get_vt(16, 9), 8, 0xFF, 0);


#define OPAVG_EMIT(x, y, ext, w)				\
Value* VexExprBinop##x::emit(void) const			\
{								\
	BINOP_SETUP						\
	v1 = builder->CreateBitCast(v1, y);			\
	v2 = builder->CreateBitCast(v2, y);			\
	v1 = builder->Create##ext(v1, w);			\
	v2 = builder->Create##ext(v2, w);			\
	Value* result = builder->CreateBitCast(			\
		get_c(y->getBitWidth(), 0), y);			\
	Value* partial = builder->CreateAdd(v1, v2);		\
	for(unsigned i = 0; i < y->getNumElements(); ++i) {	\
		Value* e = builder->CreateExtractElement(	\
			partial, get_32i(i));			\
		e = builder->CreateAdd(e, 			\
			get_c(e->getType()->			\
				getPrimitiveSizeInBits(), 1));	\
		e = builder->CreateLShr(e, 			\
			get_c(e->getType()->			\
				getPrimitiveSizeInBits(), 1));	\
		e = builder->CreateTrunc(e, 			\
			y->getScalarType());			\
		result = builder->CreateInsertElement(		\
			result, e, get_32i(i));			\
	}							\
	return result;						\
}

OPAVG_EMIT(Avg8Ux8 , get_vt(8 , 8), ZExt, get_vt(8 , 16));
OPAVG_EMIT(Avg8Ux16, get_vt(16, 8), ZExt, get_vt(16, 16));
OPAVG_EMIT(Avg8Sx16, get_vt(16, 8), SExt, get_vt(16, 16));

OPAVG_EMIT(Avg16Ux4, get_vt(4 , 16), ZExt, get_vt(4 , 32));
OPAVG_EMIT(Avg16Ux8, get_vt(8 , 16), ZExt, get_vt(8 , 32));
OPAVG_EMIT(Avg16Sx8, get_vt(8 , 16), SExt, get_vt(8 , 32));

OPAVG_EMIT(Avg32Ux4, get_vt(4 , 32), ZExt, get_vt(4 , 64));
OPAVG_EMIT(Avg32Sx4, get_vt(4 , 32), SExt, get_vt(4 , 64));


#define OPHOP_EMIT(x, vt, aop, ext)				\
Value* VexExprBinop##x::emit(void) const			\
{								\
	BINOP_SETUP						\
	int	prim_sz = vt->getBitWidth() / vt->getNumElements(); \
	v1 = builder->CreateBitCast(v1, vt);			\
	v2 = builder->CreateBitCast(v2, vt);			\
	Value* ret;						\
	ret = builder->CreateBitCast(get_c(vt->getBitWidth(), 0), vt); \
	for(unsigned i = 0; i < vt->getNumElements(); ++i) {	\
		Value	*e, *lhs, *rhs;	\
		lhs = builder->CreateExtractElement(v1, get_32i(i));	\
		rhs = builder->CreateExtractElement(v2, get_32i(i));	\
		lhs = builder->Create##ext(lhs, get_i(2*prim_sz));	\
		rhs = builder->Create##ext(rhs, get_i(2*prim_sz));	\
		e = builder->Create##aop(lhs, rhs);		\
		e = builder->CreateLShr(e, get_c(prim_sz*2, 1)); \
		e = builder->CreateTrunc(e, vt->getScalarType()); \
		ret = builder->CreateInsertElement(ret, e, get_32i(i)); \
	} \
	return builder->CreateBitCast(ret, get_i(vt->getBitWidth())); \
}

OPHOP_EMIT(HAdd16Ux2, get_vt(2, 16), Add, ZExt);
OPHOP_EMIT(HAdd16Sx2, get_vt(2, 16), Add, SExt);
OPHOP_EMIT(HSub16Ux2, get_vt(2, 16), Sub, ZExt);
OPHOP_EMIT(HSub16Sx2, get_vt(2, 16), Sub, SExt);
OPHOP_EMIT(HAdd8Ux4, get_vt(4, 8), Add, ZExt);
OPHOP_EMIT(HAdd8Sx4, get_vt(4, 8), Add, SExt);
OPHOP_EMIT(HSub8Ux4, get_vt(4, 8), Sub, ZExt);
OPHOP_EMIT(HSub8Sx4, get_vt(4, 8), Sub, SExt);

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

//note max vector elements of 16
// op(v1, splat(v2 <const i8>))
#define OPVS_EMIT(x, y, z)				\
Value* VexExprBinop##x::emit(void) const		\
{							\
	Value	*ret;					\
	BINOP_SETUP					\
	assert(y->getNumElements() <= 16);		\
	v1 = builder->CreateBitCast(v1, y);		\
	v2 = builder->CreateZExt(v2, 			\
		get_i(y->getBitWidth()));		\
	if (!isa<Constant>(v2)) {			\
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
		Constant	*cv;				\
		cv = get_cvl(shuffle_v, y->getNumElements());	\
		v2 = builder->CreateShuffleVector(v2, v2, cv);	\
	} else { \
		std::vector<Constant*>	c_v(			\
			y->getNumElements(), 			\
			builder->getFolder().CreateTruncOrBitCast(	\
				dyn_cast<Constant>(v2),		\
				get_i(y->getBitWidth()/y->getNumElements())));	\
		v2 = ConstantVector::get(c_v);			\
	} \
	ret = builder->Create##z(v1, v2);		\
	return ret;	\
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
	Value	*last_perm_idx = NULL, *last_ext_elem = NULL;		\
	BINOP_SETUP							\
	v1 = builder->CreateBitCast(v1, y);				\
	v2 = builder->CreateBitCast(v2, y);				\
	Value* result = get_c(y->getBitWidth(), 0);			\
	result = builder->CreateBitCast(result, y);			\
	for(unsigned i = 0; i < y->getNumElements(); ++i) {		\
		Value	*perm_idx;					\
		Value	*ext_elem;					\
		perm_idx = builder->CreateZExt(				\
			builder->CreateExtractElement(			\
				v2,					\
				get_32i(i)),				\
			get_i(32));					\
		if (perm_idx == last_perm_idx)				\
			ext_elem = last_ext_elem;			\
		else							\
			ext_elem = builder->CreateExtractElement(v1, perm_idx);	\
		result = builder->CreateInsertElement(		\
			result,					\
			ext_elem,				\
			get_32i(i));				\
		last_perm_idx = perm_idx;			\
		last_ext_elem = ext_elem;			\
	}						\
	return result;					\
}

OPSHUF_EMIT(Perm8x8, get_vt(8, 8), get_vt(8, 8))
OPSHUF_EMIT(Perm8x16, get_vt(16, 8), get_vt(16, 8))
