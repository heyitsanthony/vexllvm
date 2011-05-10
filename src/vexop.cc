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

/* vector stuff */
CASE_OP(32UtoV128)
CASE_OP(64HLtoV128)	// :: (I64,I64) -> I128
CASE_OP(V128to64)
CASE_OP(InterleaveLO8x16)

	case Iop_INVALID: return "Iop_INVALID";
	default:
	fprintf(stderr, "Unknown opcode %x\n", op);
	}
	return "???Op???";
}

#define UNOP_SETUP				\
	Value		*v1;			\
	IRBuilder<>	*builder;		\
	builder = theGenLLVM->getBuilder();	\
	v1 = args[0]->emit();

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

#define XHI_TO_Y_EMIT(x,y,z)			\
Value* VexExprUnop##x::emit(void) const		\
{						\
	UNOP_SETUP				\
	return builder->CreateTrunc(		\
		builder->CreateLShr(v1,		\
			ConstantInt::get(	\
				getGlobalContext(),	\
				APInt(32, y)))		\
			,			\
		builder->z());			\
}						\

XHI_TO_Y_EMIT(64HIto32, 32, getInt32Ty)

X_TO_Y_EMIT(64to32, CreateTrunc, getInt32Ty)
X_TO_Y_EMIT(32Uto64,CreateZExt, getInt64Ty)
X_TO_Y_EMIT(32Sto64, CreateSExt, getInt64Ty)
X_TO_Y_EMIT(64to1, CreateTrunc, getInt1Ty)
X_TO_Y_EMIT(64to8, CreateTrunc, getInt8Ty)
X_TO_Y_EMIT(64to16, CreateTrunc, getInt16Ty)
X_TO_Y_EMIT(1Uto8, CreateZExt, getInt8Ty)
X_TO_Y_EMIT(1Uto64, CreateZExt, getInt64Ty)
X_TO_Y_EMIT(16Uto64, CreateZExt, getInt64Ty)
X_TO_Y_EMIT(8Uto32, CreateZExt, getInt32Ty)
X_TO_Y_EMIT(8Uto64, CreateZExt, getInt64Ty)
//X_TO_Y_EMIT(V128to64, CreateTrunc, getInt64Ty)
//
UNOP_EMIT(Not1, CreateNot)


#define get_vt_8x16() VectorType::get(Type::getInt16Ty(getGlobalContext()), 8)
#define get_vt_8x8() VectorType::get(Type::getInt8Ty(getGlobalContext()), 8)
#define get_vt_4x16() VectorType::get(Type::getInt16Ty(getGlobalContext()), 4)
#define get_vt_16x8() VectorType::get(Type::getInt8Ty(getGlobalContext()), 16)

Value* VexExprUnopV128to64::emit(void) const
{
	Value		*v_trunc;

	UNOP_SETUP
	v_trunc = builder->CreateTrunc(v1, get_vt_4x16());
	return builder->CreateBitCast(v_trunc, builder->getInt64Ty(), "V128to64");
}

/* so stupid */
Value* VexExprUnop32UtoV128::emit(void) const
{
	Value		*v_128i;

	UNOP_SETUP

	v_128i = builder->CreateZExt(
		v1, IntegerType::get(getGlobalContext(), 128));
	
	return builder->CreateBitCast(v_128i, get_vt_8x16(), "32UtoV128");
}

Value* VexExprUnop64HLtoV128::emit(void) const
{
	Value		*v_128i, *v_128hl;

	UNOP_SETUP

	v_128i = builder->CreateZExt(
		v1, IntegerType::get(getGlobalContext(), 128));
	
	v_128hl = builder->CreateOr(
		v_128i,
		builder->CreateShl(
			v_128i, 
			ConstantInt::get(
				getGlobalContext(),
				APInt(32, 64))));

	return builder->CreateBitCast(v_128hl, get_vt_8x16(), "64HLtoV128");
}

/* (i32, i32) -> i64 */
Value* VexExprUnop32HLto64::emit(void) const
{
	Value		*v_64;

	UNOP_SETUP

	v_64 = builder->CreateZExt(v1, builder->getInt64Ty());
	return builder->CreateOr(
		v_64,
		builder->CreateShl(
			v_64, 
			ConstantInt::get(
				getGlobalContext(),
				APInt(32, 32))));
}

#define BINOP_EMIT(x,y)				\
Value* VexExprBinop##x::emit(void) const	\
{						\
	Value		*v1, *v2;		\
	IRBuilder<>	*builder;		\
	v1 = args[0]->emit();			\
	v2 = args[1]->emit();			\
	builder = theGenLLVM->getBuilder();	\
	return builder->Create##y(v1, v2);	\
}

BINOP_EMIT(Add8, Add)
BINOP_EMIT(Add16, Add)
BINOP_EMIT(Add32, Add)
BINOP_EMIT(Add64, Add)

BINOP_EMIT(And8, And)
BINOP_EMIT(And16, And)
BINOP_EMIT(And32, And)
BINOP_EMIT(And64, And)

BINOP_EMIT(Or8, Or)
BINOP_EMIT(Or16, Or)
BINOP_EMIT(Or32, Or)
BINOP_EMIT(Or64, Or)

BINOP_EMIT(Shl8, Shl)
BINOP_EMIT(Shl16, Shl)
BINOP_EMIT(Shl32, Shl)
BINOP_EMIT(Shl64, Shl)

BINOP_EMIT(Shr8, LShr)
BINOP_EMIT(Shr16, LShr)
BINOP_EMIT(Shr32, LShr)
BINOP_EMIT(Shr64, LShr)

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

Value* VexExprBinopCmpEQ8x16::emit(void) const
{
	Value		*v1, *v2, *cmp_8x1;
	IRBuilder<>	*builder;
	v1 = args[0]->emit();
	v2 = args[1]->emit();
	builder = theGenLLVM->getBuilder();
	cmp_8x1 = builder->CreateICmpEQ(v1, v2);
	return builder->CreateSExt(cmp_8x1, get_vt_8x16());
}

Value* VexExprBinopSub8x16::emit(void) const
{
	Value		*v1, *v2;
	IRBuilder<>	*builder;

	builder = theGenLLVM->getBuilder();
	v1 = builder->CreateBitCast(args[0]->emit(), get_vt_16x8());
	v2 = builder->CreateBitCast(args[1]->emit(), get_vt_16x8());
	return builder->CreateSub(v1, v2);
}

BINOP_EMIT(CmpLE64S, ICmpSLE)
BINOP_EMIT(CmpLE64U, ICmpULE)
BINOP_EMIT(CmpLT64U, ICmpULT)

/* count number of zeros (from bit0) leading up to first 1. */
/* 0x0 -> undef */
/* 0x1 -> 1 */
/* 0x2 -> 2 */
/* 0x3 -> 1 */
/* 0x4 -> 3 */
Value* VexExprUnopCtz64::emit(void) const
{
	IRBuilder<>     *builder = theGenLLVM->getBuilder();
	llvm::Function	*f;
	llvm::Value	*v;

	v = args[0]->emit();
	f = theVexHelpers->getHelper("vexop_ctz64");
	assert (f != NULL);

	return builder->CreateCall(f, v);
}

#define get_i32(x) ConstantInt::get(getGlobalContext(), APInt(32, x))

/* interleave 16 elements of an 8-bit width */
Value* VexExprBinopInterleaveLO8x16::emit(void) const
{
	IRBuilder<>     *builder = theGenLLVM->getBuilder();
	llvm::Value	*v1, *v2;
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

	v1 = args[0]->emit();
	v2 = args[1]->emit();
	v1 = builder->CreateBitCast(v1, get_vt_16x8(), "lo8x16_v1");
	v2 = builder->CreateBitCast(v2, get_vt_16x8(), "lo8x16_v2");

	cv = ConstantVector::get(
		std::vector<Constant*>(
			shuffle_v,
			shuffle_v + sizeof(shuffle_v)/sizeof(Constant*)));

	return builder->CreateShuffleVector(v1, v2, cv, "ILO8x16");
}
