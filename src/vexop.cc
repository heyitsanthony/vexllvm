#include <stdio.h>

#include "genllvm.h"
#include "vexop.h"

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

/* fuck this)
 I'm tired */
#if 0
      /* ------ Floating point.  We try to be IEEE754 compliant. ------ */

      /* --- Simple stuff as mandated by 754. --- */

      /* Binary operations)
 with rounding. */
      /* :: IRRoundingMode(I32) x F64 x F64 -> F64 */ 
      Iop_AddF64)
 Iop_SubF64, Iop_MulF64, Iop_DivF64,

      /* :: IRRoundingMode(I32) x F32 x F32 -> F32 */ 
      Iop_AddF32)
 Iop_SubF32, Iop_MulF32, Iop_DivF32,

      /* Variants of the above which produce a 64-bit result but which
         round their result to a IEEE float range first. */
      /* :: IRRoundingMode(I32) x F64 x F64 -> F64 */ 
      Iop_AddF64r32, Iop_SubF64r32, Iop_MulF64r32, Iop_DivF64r32, 

      /* Unary operations, without rounding. */
      /* :: F64 -> F64 */
      Iop_NegF64, Iop_AbsF64,

      /* :: F32 -> F32 */
      Iop_NegF32, Iop_AbsF32,

      /* Unary operations, with rounding. */
      /* :: IRRoundingMode(I32) x F64 -> F64 */
      Iop_SqrtF64, Iop_SqrtF64r32,

      /* :: IRRoundingMode(I32) x F32 -> F32 */
      Iop_SqrtF32,

      /* Comparison, yielding GT/LT/EQ/UN(ordered), as per the following:
            0x45 Unordered
            0x01 LT
            0x00 GT
            0x40 EQ
         This just happens to be the Intel encoding.  The values
         are recorded in the type IRCmpF64Result.
      */
      /* :: F64 x F64 -> IRCmpF64Result(I32) */
      Iop_CmpF64,
#endif


const char* getVexOpName(IROp op)
{
	switch(op) {
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


	case Iop_INVALID: return "Iop_INVALID";
	default:
	fprintf(stderr, "Unknown opcode %x\n", op);
	}
	return "???Op???";
}

Value* VexExprBinopAdd64::emit(void) const
{
	Value	*v1, *v2;
	v1 = args[0]->emit();
	v2 = args[1]->emit();
	return (theGenLLVM->getBuilder())->CreateAdd(v1, v2);
}

Value* VexExprBinopSub64::emit(void) const
{
	Value	*v1, *v2;
	v1 = args[0]->emit();
	v2 = args[1]->emit();
	return (theGenLLVM->getBuilder())->CreateSub(v1, v2);
}

Value* VexExprBinopAnd64::emit(void) const
{
	Value	*v1, *v2;
	v1 = args[0]->emit();
	v2 = args[1]->emit();
	return (theGenLLVM->getBuilder())->CreateAnd(v1, v2);
}

Value* VexExprUnop32Uto64::emit(void) const
{
	Value		*v1;
	IRBuilder<>	*builder;

	v1 = args[0]->emit();
	builder = theGenLLVM->getBuilder();
	return builder->CreateBitCast(v1, builder->getInt64Ty());
}

Value* VexExprUnop64to32::emit(void) const
{
	Value		*v1;
	IRBuilder<>	*builder;

	v1 = args[0]->emit();
	builder = theGenLLVM->getBuilder();
	return builder->CreateBitCast(v1, builder->getInt32Ty());
}


Value* VexExprBinopCmpEQ64::emit(void) const
{
	Value		*v_1, *v_2;
	IRBuilder<>	*builder;

	v_1 = args[0]->emit();
	v_2 = args[1]->emit();
	builder = theGenLLVM->getBuilder();

	return builder->CreateICmp(llvm::CmpInst::ICMP_EQ, v_1, v_2);
}
