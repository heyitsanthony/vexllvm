#include <llvm/IR/Type.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include "genllvm.h"
#include "Sugar.h"
#include <stdio.h>
#include "vexsb.h"
#include "vexstmt.h"
#include "vexop.h"
#include "vexhelpers.h"

#include "vexexpr.h"

using namespace llvm;

unsigned int VexExpr::vex_expr_op_count[VEX_MAX_OP];

static IROp getNaryOp(const IRExpr* expr)
{
	switch (expr->tag) {
	case Iex_Unop: return expr->Iex.Unop.op;
	case Iex_Binop:return expr->Iex.Binop.op;
	case Iex_Triop:return expr->Iex.Triop.details->op;
	case Iex_Qop: return expr->Iex.Qop.details->op;
	default:
		assert (0 == 1 && "BAD TAG");
	}
	return (IROp)~0;
}

static const IRExpr** getNaryArgs(const IRExpr* expr)
{
	switch (expr->tag) {
	case Iex_Unop: return (const IRExpr**)((void*)(&expr->Iex.Unop.arg));
	case Iex_Binop:return (const IRExpr**)((void*)(&expr->Iex.Binop.arg1));
	case Iex_Triop:
	case Iex_Qop:
		return (const IRExpr**)((void*)(&expr->Iex.Triop.details->arg1));
	default:
		assert (0 == 1 && "BAD TAG");
	}
	return NULL;
}

VexExpr* VexExpr::create(VexStmt* in_parent, const IRExpr* expr)
{
	VexExpr	*ret = NULL;

	switch (expr->tag) {
#define EXPR_TAGOP(x) case Iex_##x :		\
	ret = new VexExpr##x(in_parent, expr);	\
	break;

	EXPR_TAGOP(Get);
	EXPR_TAGOP(GetI);
	EXPR_TAGOP(RdTmp);
	case Iex_Qop:
	case Iex_Triop:
	case Iex_Binop:
	case Iex_Unop:
		ret = VexExprNaryOp::createOp(in_parent, expr);
		break;

	EXPR_TAGOP(Load);
	case Iex_Const:
		ret = VexExprConst::createConst(in_parent, expr);
		break;

	EXPR_TAGOP(Mux0X);
	EXPR_TAGOP(CCall);
	case Iex_Binder:
	default:
		fprintf(stderr, "Expr: ??? %x\n", expr->tag);
	}

	return ret;
}

VexExpr* VexExprNaryOp::createOp(VexStmt* in_parent, const IRExpr* expr)
{
	IROp		op;
	unsigned int	op_idx;

	op = getNaryOp(expr);
	op_idx = op - Iop_INVALID;

	assert (op_idx < VEX_MAX_OP && "Op is larger than VEX_MAX_OP - OFLOW");
	VexExpr::vex_expr_op_count[op_idx]++;

	/* known ops */
	switch (op) {
#define TRIOP_TAGOP(x) case Iop_##x : \
return new VexExprTriop##x(in_parent, expr)

#define BINOP_TAGOP(x) case Iop_##x : \
return new VexExprBinop##x(in_parent, expr)

#define UNOP_TAGOP(x) case Iop_##x : \
return new VexExprUnop##x(in_parent, expr)

	BINOP_TAGOP(CmpEQ8);
	BINOP_TAGOP(CmpEQ16);
	BINOP_TAGOP(CmpEQ32);
	BINOP_TAGOP(CmpEQ64);

	BINOP_TAGOP(CmpNE8);
	BINOP_TAGOP(CmpNE16);
	BINOP_TAGOP(CmpNE32);
	BINOP_TAGOP(CmpNE64);
	BINOP_TAGOP(CmpF64);

	BINOP_TAGOP(CasCmpEQ8);
	BINOP_TAGOP(CasCmpEQ16);
	BINOP_TAGOP(CasCmpEQ32);
	BINOP_TAGOP(CasCmpEQ64);

	BINOP_TAGOP(CasCmpNE8);
	BINOP_TAGOP(CasCmpNE16);
	BINOP_TAGOP(CasCmpNE32);
	BINOP_TAGOP(CasCmpNE64);

	BINOP_TAGOP(CmpLE64S);
	BINOP_TAGOP(CmpLE64U);
	BINOP_TAGOP(CmpLT64S);
	BINOP_TAGOP(CmpLT64U);

	BINOP_TAGOP(CmpLE32S);
	BINOP_TAGOP(CmpLE32U);
	BINOP_TAGOP(CmpLT32S);
	BINOP_TAGOP(CmpLT32U);

	BINOP_TAGOP(Add8);
	BINOP_TAGOP(Add16);
	BINOP_TAGOP(Add32);
	BINOP_TAGOP(Add64);

	BINOP_TAGOP(And8);
	BINOP_TAGOP(And16);
	BINOP_TAGOP(And32);
	BINOP_TAGOP(And64);
	BINOP_TAGOP(AndV128);

	BINOP_TAGOP(DivModU128to64);
	BINOP_TAGOP(DivModS128to64);
	BINOP_TAGOP(DivModU64to32);
	BINOP_TAGOP(DivModS64to32);

	BINOP_TAGOP(Mul64F0x2);
	BINOP_TAGOP(Div64F0x2);
	BINOP_TAGOP(Add64F0x2);
	BINOP_TAGOP(Sub64F0x2);
	BINOP_TAGOP(Mul32F0x4);
	BINOP_TAGOP(Div32F0x4);
	BINOP_TAGOP(Add32F0x4);
	BINOP_TAGOP(Sub32F0x4);

	UNOP_TAGOP(Sqrt32F0x4);
	UNOP_TAGOP(Recip32F0x4);
	UNOP_TAGOP(RSqrt32F0x4);
	UNOP_TAGOP(Sqrt64F0x2);
	UNOP_TAGOP(Recip64F0x2);
	UNOP_TAGOP(RSqrt64F0x2);

	BINOP_TAGOP(Min64F0x2);
	BINOP_TAGOP(Max32F0x4);
	BINOP_TAGOP(Min32F0x4);
	BINOP_TAGOP(Max64F0x2);

	BINOP_TAGOP(CmpLT32F0x4);
	BINOP_TAGOP(CmpLE32F0x4);
	BINOP_TAGOP(CmpEQ32F0x4);
	BINOP_TAGOP(CmpUN32F0x4);

	BINOP_TAGOP(CmpLT64F0x2);
	BINOP_TAGOP(CmpLE64F0x2);
	BINOP_TAGOP(CmpEQ64F0x2);
	BINOP_TAGOP(CmpUN64F0x2);
	
	BINOP_TAGOP(SetV128lo64);
	BINOP_TAGOP(SetV128lo32);

	BINOP_TAGOP(InterleaveLO8x8);
	BINOP_TAGOP(InterleaveHI8x8);
	BINOP_TAGOP(InterleaveLO8x16);
	BINOP_TAGOP(InterleaveHI8x16);
	BINOP_TAGOP(InterleaveLO16x4);
	BINOP_TAGOP(InterleaveHI16x4);
	BINOP_TAGOP(InterleaveLO16x8);
	BINOP_TAGOP(InterleaveHI16x8);
	BINOP_TAGOP(InterleaveLO64x2);
	BINOP_TAGOP(InterleaveHI64x2);
	BINOP_TAGOP(InterleaveLO32x4);
	BINOP_TAGOP(InterleaveHI32x4);
	BINOP_TAGOP(InterleaveLO32x2);
	BINOP_TAGOP(InterleaveHI32x2);

	BINOP_TAGOP(Mul8);
	BINOP_TAGOP(Mul16);
	BINOP_TAGOP(Mul32);
	BINOP_TAGOP(Mul64);

	BINOP_TAGOP(MullS8);
	BINOP_TAGOP(MullS16);
	BINOP_TAGOP(MullS32);
	BINOP_TAGOP(MullS64);

	BINOP_TAGOP(MullU8);
	BINOP_TAGOP(MullU16);
	BINOP_TAGOP(MullU32);
	BINOP_TAGOP(MullU64);

	BINOP_TAGOP(Max8Ux8);
	BINOP_TAGOP(Max8Ux16);
	BINOP_TAGOP(Max16Sx4);
	BINOP_TAGOP(Max16Sx8);

	BINOP_TAGOP(Min8Ux8);
	BINOP_TAGOP(Min8Ux16);
	BINOP_TAGOP(Min16Sx4);
	BINOP_TAGOP(Min16Sx8);

	BINOP_TAGOP(Min32Fx2);
	BINOP_TAGOP(Max32Fx2);
	BINOP_TAGOP(Min32Fx4);
	BINOP_TAGOP(Max32Fx4);

	BINOP_TAGOP(Max64Fx2);
	BINOP_TAGOP(Min64Fx2);

	UNOP_TAGOP(Sqrt32Fx4);
	UNOP_TAGOP(Sqrt64Fx2);

	UNOP_TAGOP(RSqrt32Fx4);
	UNOP_TAGOP(RSqrt64Fx2);

	UNOP_TAGOP(Recip32Fx4);
	UNOP_TAGOP(Recip64Fx2);

	BINOP_TAGOP(Or8);
	BINOP_TAGOP(Or16);
	BINOP_TAGOP(Or32);
	BINOP_TAGOP(Or64);
	BINOP_TAGOP(OrV128);

	BINOP_TAGOP(Shl8);
	BINOP_TAGOP(Shl16);
	BINOP_TAGOP(Shl32);
	BINOP_TAGOP(Shl64);

	BINOP_TAGOP(Sar8);
	BINOP_TAGOP(Sar16);
	BINOP_TAGOP(Sar32);
	BINOP_TAGOP(Sar64);

	BINOP_TAGOP(Shr8);
	BINOP_TAGOP(Shr16);
	BINOP_TAGOP(Shr32);
	BINOP_TAGOP(Shr64);

	BINOP_TAGOP(Sub8);
	BINOP_TAGOP(Sub16);
	BINOP_TAGOP(Sub32);
	BINOP_TAGOP(Sub64);

	BINOP_TAGOP(Xor8);
	BINOP_TAGOP(Xor16);
	BINOP_TAGOP(Xor32);
	BINOP_TAGOP(Xor64);
	BINOP_TAGOP(XorV128);

	UNOP_TAGOP(Not1);
	UNOP_TAGOP(Not8);
	UNOP_TAGOP(Not16);
	UNOP_TAGOP(Not32);
	UNOP_TAGOP(Not64);
	UNOP_TAGOP(NotV128);

	UNOP_TAGOP(Dup8x16);

	UNOP_TAGOP(1Uto8);
	UNOP_TAGOP(1Uto32);
	UNOP_TAGOP(1Uto64);
	UNOP_TAGOP(8Uto16);
	UNOP_TAGOP(8Sto16);
	UNOP_TAGOP(8Uto32);
	UNOP_TAGOP(8Sto32);
	UNOP_TAGOP(8Uto64);
	UNOP_TAGOP(8Sto64);
	UNOP_TAGOP(16Uto64);
	UNOP_TAGOP(16Sto64);
	UNOP_TAGOP(16Uto32);
	UNOP_TAGOP(16to8);
	UNOP_TAGOP(16Sto32);
	UNOP_TAGOP(32Uto64);
	UNOP_TAGOP(32Sto64);
	UNOP_TAGOP(32to8);
	UNOP_TAGOP(32to16);
	UNOP_TAGOP(64to32);
	UNOP_TAGOP(64to1);
	UNOP_TAGOP(32to1);
	UNOP_TAGOP(64to8);
	UNOP_TAGOP(64to16);
	UNOP_TAGOP(32UtoV128);
	UNOP_TAGOP(64UtoV128);
	UNOP_TAGOP(V128to64);
	UNOP_TAGOP(V128HIto64);
	UNOP_TAGOP(128to64);
	UNOP_TAGOP(128HIto64);
	BINOP_TAGOP(16HLto32);
	BINOP_TAGOP(32HLto64);
	BINOP_TAGOP(64HLtoV128);
	BINOP_TAGOP(64HLto128);
	UNOP_TAGOP(16HIto8);
	UNOP_TAGOP(32HIto16);
	UNOP_TAGOP(64HIto32);
	UNOP_TAGOP(F32toF64);

	UNOP_TAGOP(AbsF32);
	UNOP_TAGOP(AbsF64);

	BINOP_TAGOP(F64toF32);
	UNOP_TAGOP(I32StoF64);
	UNOP_TAGOP(I32UtoF64);

	UNOP_TAGOP(ReinterpF64asI64);
	UNOP_TAGOP(ReinterpI64asF64);
	UNOP_TAGOP(ReinterpF32asI32);
	UNOP_TAGOP(ReinterpI32asF32);

	BINOP_TAGOP(GetElem8x8);
	BINOP_TAGOP(GetElem32x2);

	BINOP_TAGOP(I64StoF64);
	BINOP_TAGOP(I64UtoF64);
	BINOP_TAGOP(F64toI32S);
	BINOP_TAGOP(F64toI32U);
	BINOP_TAGOP(F64toI64S);


	UNOP_TAGOP(Ctz32);
	UNOP_TAGOP(Ctz64);
	UNOP_TAGOP(Clz64);
	UNOP_TAGOP(Clz32);

	BINOP_TAGOP(Shl8x8);
	BINOP_TAGOP(Shr8x8);
	BINOP_TAGOP(Sar8x8);
	BINOP_TAGOP(Sal8x8);

	BINOP_TAGOP(Shl8x16);
	BINOP_TAGOP(Shr8x16);
	BINOP_TAGOP(Sar8x16);
	BINOP_TAGOP(Sal8x16);

	BINOP_TAGOP(Shl16x4);
	BINOP_TAGOP(Shr16x4);
	BINOP_TAGOP(Sar16x4);
	BINOP_TAGOP(Sal16x4);

	BINOP_TAGOP(Shl16x8);
	BINOP_TAGOP(Shr16x8);
	BINOP_TAGOP(Sar16x8);
	BINOP_TAGOP(Sal16x8);

	BINOP_TAGOP(Shl32x2);
	BINOP_TAGOP(Shr32x2);
	BINOP_TAGOP(Sar32x2);
	BINOP_TAGOP(Sal32x2);

	BINOP_TAGOP(Shl32x4);
	BINOP_TAGOP(Shr32x4);
	BINOP_TAGOP(Sar32x4);
	BINOP_TAGOP(Sal32x4);

	BINOP_TAGOP(SarN8x8);
	BINOP_TAGOP(ShlN8x8);
	BINOP_TAGOP(ShrN8x8);

	BINOP_TAGOP(SarN8x16);
	BINOP_TAGOP(ShlN8x16);
	BINOP_TAGOP(ShrN8x16);

	BINOP_TAGOP(ShlN16x4);
	BINOP_TAGOP(ShrN16x4);
	BINOP_TAGOP(SarN16x4);

	BINOP_TAGOP(ShlN16x8);
	BINOP_TAGOP(ShrN16x8);
	BINOP_TAGOP(SarN16x8);

	BINOP_TAGOP(ShlN32x2);
	BINOP_TAGOP(ShrN32x2);
	BINOP_TAGOP(SarN32x2);

	BINOP_TAGOP(ShlN32x4);
	BINOP_TAGOP(ShrN32x4);
	BINOP_TAGOP(SarN32x4);

	BINOP_TAGOP(ShlN64x2);
	BINOP_TAGOP(ShrN64x2);
	BINOP_TAGOP(SarN64x2);

	BINOP_TAGOP(Add8x8);
	BINOP_TAGOP(Add8x16);
	BINOP_TAGOP(Add16x4);
	BINOP_TAGOP(Add16x8);
	BINOP_TAGOP(Add32x2);
	BINOP_TAGOP(Add32x4);
	BINOP_TAGOP(Add64x2);

	BINOP_TAGOP(Sub8x8);
	BINOP_TAGOP(Sub8x16);
	BINOP_TAGOP(Sub16x4);
	BINOP_TAGOP(Sub16x8);
	BINOP_TAGOP(Sub32x2);
	BINOP_TAGOP(Sub32x4);
	BINOP_TAGOP(Sub64x2);

	BINOP_TAGOP(Mul8x8);
	BINOP_TAGOP(Mul8x16);
	BINOP_TAGOP(Mul16x4);
	BINOP_TAGOP(Mul16x8);
	BINOP_TAGOP(Mul32x2);
	BINOP_TAGOP(Mul32x4);

	BINOP_TAGOP(MulHi16Ux4);
	BINOP_TAGOP(MulHi16Sx4);
	BINOP_TAGOP(MulHi16Ux8);
	BINOP_TAGOP(MulHi16Sx8);
	BINOP_TAGOP(MulHi32Ux4);
	BINOP_TAGOP(MulHi32Sx4);


	BINOP_TAGOP(NarrowBin16to8x16);
	BINOP_TAGOP(NarrowBin32to16x8);
	BINOP_TAGOP(QNarrowBin16Sto8Ux8);
	BINOP_TAGOP(QNarrowBin16Sto8Ux16);
	BINOP_TAGOP(QNarrowBin32Sto16Ux8);
	BINOP_TAGOP(QNarrowBin16Sto8Sx16);
	BINOP_TAGOP(QNarrowBin32Sto16Sx8);
	BINOP_TAGOP(QNarrowUn16Sto8Ux8);
	BINOP_TAGOP(QNarrowBin16Sto8Sx8);
	BINOP_TAGOP(QNarrowBin32Sto16Sx4);

	BINOP_TAGOP(QAdd64Sx1);
	BINOP_TAGOP(QAdd64Sx2);
	BINOP_TAGOP(QAdd64Ux1);
	BINOP_TAGOP(QAdd64Ux2);

	BINOP_TAGOP(QAdd32Sx2);
	BINOP_TAGOP(QAdd32Sx4);
	BINOP_TAGOP(QAdd32Ux2);
	BINOP_TAGOP(QAdd32Ux4);
	
	BINOP_TAGOP(QAdd16Sx2);
	BINOP_TAGOP(QAdd16Sx4);
	BINOP_TAGOP(QAdd16Sx8);
	BINOP_TAGOP(QAdd16Ux2);
	BINOP_TAGOP(QAdd16Ux4);
	BINOP_TAGOP(QAdd16Ux8);

	BINOP_TAGOP(QAdd8Sx4);
	BINOP_TAGOP(QAdd8Sx8);
	BINOP_TAGOP(QAdd8Sx16);
	BINOP_TAGOP(QAdd8Ux4);
	BINOP_TAGOP(QAdd8Ux8);
	BINOP_TAGOP(QAdd8Ux16);

	BINOP_TAGOP(QSub64Sx1);
	BINOP_TAGOP(QSub64Sx2);
	BINOP_TAGOP(QSub64Ux1);
	BINOP_TAGOP(QSub64Ux2);

	BINOP_TAGOP(QSub32Sx2);
	BINOP_TAGOP(QSub32Sx4);
	BINOP_TAGOP(QSub32Ux2);
	BINOP_TAGOP(QSub32Ux4);

	BINOP_TAGOP(QSub16Sx2);
	BINOP_TAGOP(QSub16Sx4);
	BINOP_TAGOP(QSub16Sx8);
	BINOP_TAGOP(QSub16Ux2);
	BINOP_TAGOP(QSub16Ux4);
	BINOP_TAGOP(QSub16Ux8);

	BINOP_TAGOP(QSub8Sx4);
	BINOP_TAGOP(QSub8Sx8);
	BINOP_TAGOP(QSub8Sx16);
	BINOP_TAGOP(QSub8Ux4);
	BINOP_TAGOP(QSub8Ux8);
	BINOP_TAGOP(QSub8Ux16);

	BINOP_TAGOP(Avg8Ux8); 
	BINOP_TAGOP(Avg8Ux16);
	BINOP_TAGOP(Avg8Sx16);
	BINOP_TAGOP(Avg16Ux4);
	BINOP_TAGOP(Avg16Ux8);
	BINOP_TAGOP(Avg16Sx8);
	BINOP_TAGOP(Avg32Ux4);
	BINOP_TAGOP(Avg32Sx4);

	BINOP_TAGOP(HAdd16Ux2);
	BINOP_TAGOP(HAdd16Sx2);
	BINOP_TAGOP(HSub16Ux2);
	BINOP_TAGOP(HSub16Sx2);
	BINOP_TAGOP(HAdd8Ux4);
	BINOP_TAGOP(HAdd8Sx4);
	BINOP_TAGOP(HSub8Ux4);
	BINOP_TAGOP(HSub8Sx4);

	BINOP_TAGOP(Add32Fx2);
	BINOP_TAGOP(Add32Fx4);
	BINOP_TAGOP(Add64Fx2);

	BINOP_TAGOP(Sub32Fx2);
	BINOP_TAGOP(Sub32Fx4);
	BINOP_TAGOP(Sub64Fx2);

	BINOP_TAGOP(Mul32Fx2);
	BINOP_TAGOP(Mul32Fx4);
	BINOP_TAGOP(Mul64Fx2);

	BINOP_TAGOP(Div32Fx4);
	BINOP_TAGOP(Div64Fx2);

	TRIOP_TAGOP(AddF64);
	TRIOP_TAGOP(SubF64);
	TRIOP_TAGOP(MulF64);
	TRIOP_TAGOP(DivF64);
	UNOP_TAGOP(NegF64);

	TRIOP_TAGOP(AddF32);
	TRIOP_TAGOP(SubF32);
	TRIOP_TAGOP(MulF32);
	TRIOP_TAGOP(DivF32);
	UNOP_TAGOP(NegF32);

	BINOP_TAGOP(RoundF32toInt);
	BINOP_TAGOP(RoundF64toInt);

	BINOP_TAGOP(CmpEQ8x8);
	BINOP_TAGOP(CmpEQ8x16);
	BINOP_TAGOP(CmpEQ16x4);
	BINOP_TAGOP(CmpEQ16x8);
	BINOP_TAGOP(CmpEQ32x2);
	BINOP_TAGOP(CmpEQ32x4);

	BINOP_TAGOP(CmpGT8Sx8);
	BINOP_TAGOP(CmpGT8Sx16);
	BINOP_TAGOP(CmpGT16Sx4);
	BINOP_TAGOP(CmpGT16Sx8);
	BINOP_TAGOP(CmpGT32Sx2);
	BINOP_TAGOP(CmpGT32Sx4);
	BINOP_TAGOP(CmpGT64Sx2);

	BINOP_TAGOP(CmpGT8Ux8);
	BINOP_TAGOP(CmpGT8Ux16);
	BINOP_TAGOP(CmpGT16Ux4);
	BINOP_TAGOP(CmpGT16Ux8);
	BINOP_TAGOP(CmpGT32Ux2);
	BINOP_TAGOP(CmpGT32Ux4);

	BINOP_TAGOP(CmpEQ32Fx2);
	BINOP_TAGOP(CmpEQ32Fx4);
	BINOP_TAGOP(CmpEQ64Fx2);

	BINOP_TAGOP(CmpGT32Fx2);
	BINOP_TAGOP(CmpGT32Fx4);

	BINOP_TAGOP(CmpGE32Fx2);
	BINOP_TAGOP(CmpGE32Fx4);

	BINOP_TAGOP(CmpLT32Fx4);
	BINOP_TAGOP(CmpLT64Fx2);

	BINOP_TAGOP(CmpLE32Fx4);
	BINOP_TAGOP(CmpLE64Fx2);

	BINOP_TAGOP(CmpUN32Fx4);
	BINOP_TAGOP(CmpUN64Fx2);
	BINOP_TAGOP(Perm8x8);
	BINOP_TAGOP(Perm8x16);
	
	TRIOP_TAGOP(PRemF64);
	TRIOP_TAGOP(PRemC3210F64);

	TRIOP_TAGOP(SetElem8x8);
	TRIOP_TAGOP(SetElem32x2);

	default:
		fprintf(stderr, "createOp: UNKNOWN OP %s (%x)\n",
			getVexOpName(op),
			op);
/* always works-- doesn't use our shady tables, but can't print nicely! */
//		ppIROp(op);
//		fprintf(stderr, "\n");
		break;
	}

	/* unhandled ops */
	switch (expr->tag) {
	case Iex_Qop: return new VexExprQop(in_parent, expr);
	case Iex_Triop: return new VexExprTriop(in_parent, expr);
	case Iex_Binop: return new VexExprBinop(in_parent, expr);
	case Iex_Unop: return new VexExprUnop(in_parent, expr);
	default:
		fprintf(stderr, "createOP but not N-OP?\n");
		break;
	}
	return NULL;
}

VexExprConst* VexExprConst::createConst(
	VexStmt* in_parent, const IRExpr* expr)
{
	switch (expr->Iex.Const.con->tag) {
	#define CONST_TAGOP(x) case Ico_##x : \
	return new VexExprConst##x(in_parent, expr)
	CONST_TAGOP(U1);
	CONST_TAGOP(U8);
	CONST_TAGOP(U16);
	CONST_TAGOP(U32);
	CONST_TAGOP(U64);
//	CONST_TAGOP(F32);
//	CONST_TAGOP(F32i); 3.7.0
	CONST_TAGOP(F64);
	CONST_TAGOP(F64i);
	CONST_TAGOP(V128);
	default: break;
	}
	return NULL;
}

#define EMIT_CONST_INT(x,y)				\
Value* VexExprConst##x::emit(void) const { 		\
	return ConstantInt::get(			\
		getGlobalContext(), APInt(y, x)); }
EMIT_CONST_INT(U1, 1)
EMIT_CONST_INT(U8, 8)
EMIT_CONST_INT(U16, 16)
EMIT_CONST_INT(U32, 32)
EMIT_CONST_INT(U64, 64)

static uint64_t bitmask8_to_bytemask64 ( uint8_t w8 )
{
   uint64_t w64 = 0;
   int i;
   for (i = 0; i < 8; i++) {
      if (w8 & (1<<i))
         w64 |= (0xFFULL << (8 * i));
   }
   return w64;
}

Value* VexExprConstV128::emit(void) const
{
	using namespace llvm;
	Constant	* data[] = {
		ConstantInt::get(getGlobalContext(),
			APInt(64, bitmask8_to_bytemask64((V128 >> 0) & 0xFF))),
		ConstantInt::get(getGlobalContext(),
			APInt(64, bitmask8_to_bytemask64((V128 >> 8) & 0xFF))),
	};
	Value	*cv = ConstantVector::get(
		std::vector<Constant*>(
			data,
			data + sizeof(data)/sizeof(Constant*)));
			
	return theGenLLVM->to16x8i(cv);
}

// 3.7.0
#if 0
Value* VexExprConstF32::emit(void) const {
	return ConstantFP::get(
		getGlobalContext(),
		APFloat(F32)); }
#endif

Value* VexExprConstF64i::emit(void) const
{
	return ConstantFP::get(getGlobalContext(), APFloat(x));
}

Value* VexExprConstF64::emit(void) const
{
	return ConstantFP::get(getGlobalContext(), APFloat(F64));
}

void VexExprGet::print(std::ostream& os) const
{ os << "GET(" << offset << "):" << VexSB::getTypeStr(ty); }


VexExprGetI::VexExprGetI(VexStmt* in_parent, const IRExpr* in_expr)
  : VexExpr(in_parent),
  base(in_expr->Iex.GetI.descr->base),
  len(in_expr->Iex.GetI.descr->nElems),
  ix_expr(VexExpr::create(in_parent, in_expr->Iex.GetI.ix)),
  bias(in_expr->Iex.GetI.bias)
{
	elem_type = theGenLLVM->vexTy2LLVM(in_expr->Iex.GetI.descr->elemTy);
}

VexExprGetI::~VexExprGetI(void)
{
	delete ix_expr;
}

llvm::Value* VexExprGetI::emit(void) const
{
	Value	*ix_v;
	ix_v = ix_expr->emit();
	return theGenLLVM->readCtx(base, bias, len, ix_v, elem_type);
}

void VexExprGetI::print(std::ostream& os) const
{
	os << "GetI(" << base << ", "
		<< bias << ", ";
	ix_expr->print(os);
	os << ", " << len << ")";
}

void VexExprRdTmp::print(std::ostream& os) const
{
	os << "RdTmp(t" << tmp_reg << ")";
}

Value* VexExprRdTmp::emit(void) const
{
	const VexStmt	*stmt = getParent();
	const VexSB	*sb = stmt->getParent();
	return sb->getRegValue(tmp_reg);
}

VexExprNaryOp::VexExprNaryOp(
	VexStmt* in_parent, const IRExpr* expr, unsigned int in_n_ops)
: VexExpr(in_parent)
, op(getNaryOp(expr))
, n_ops(in_n_ops)
{
	const IRExpr**	ir_exprs;

	ir_exprs = getNaryArgs(expr);
	args = new VexExpr*[n_ops];
	for (unsigned int i = 0; i < n_ops; i++)
		args[i] = VexExpr::create(in_parent, ir_exprs[i]);
}

VexExprNaryOp::~VexExprNaryOp()
{
	for (unsigned int i = 0; i < n_ops; i++)
		delete args[i];
	delete [] args;
}

void VexExprNaryOp::print(std::ostream& os) const
{
	os << getVexOpName(op) << "(";
	for (unsigned int i = 0; i < n_ops-1; i++) {
		args[i]->print(os);
		os << ", ";
	}
	args[n_ops-1]->print(os);
	os << ")";
}

VexExprLoad::VexExprLoad(VexStmt* in_parent, const IRExpr* expr)
: VexExpr(in_parent)
{
	little_endian = expr->Iex.Load.end == Iend_LE;
	assert (little_endian);

	ty = expr->Iex.Load.ty;
	addr = VexExpr::create(in_parent, expr->Iex.Load.addr);
}

VexExprLoad::~VexExprLoad(void)
{
	delete addr;
}

Value* VexExprLoad::emit(void) const
{
	Value	*addr_expr;
	addr_expr = addr->emit();
	return theGenLLVM->load(addr_expr, ty);
}

void VexExprLoad::print(std::ostream& os) const
{
	os << "Load(";
	addr->print(os);
	os << "):" << VexSB::getTypeStr(ty);
}

VexExprCCall::VexExprCCall(VexStmt* in_parent, const IRExpr* expr)
: VexExpr(in_parent)
{
	const IRCallee*	callee = expr->Iex.CCall.cee;

	for (int i = 0; expr->Iex.CCall.args[i]; i++) {
		args.push_back(VexExpr::create(
			in_parent,
			expr->Iex.CCall.args[i]));
	}

	func = theVexHelpers->getHelper(callee->name);
	assert (func != NULL && "No helper func for CCall. Ack.");
}


Value* VexExprCCall::emit(void) const
{
	std::vector<Value*>	args_v;

	foreach (it, args.begin(), args.end())
		args_v.push_back((*it)->emit());

	return theGenLLVM->getBuilder()->CreateCall(
		func, llvm::ArrayRef<llvm::Value*>(args_v));
}

void VexExprCCall::print(std::ostream& os) const
{
	os << "CCall(";
	foreach (it, args.begin(), args.end()) {
		if (it != args.begin()) os << ", ";
		(*it)->print(os);
	}
	os << ")";
}

VexExprMux0X::VexExprMux0X(VexStmt* in_parent, const IRExpr* expr)
: VexExpr(in_parent),
  cond(VexExpr::create(in_parent, expr->Iex.Mux0X.cond)),
  expr0(VexExpr::create(in_parent, expr->Iex.Mux0X.expr0)),
  exprX(VexExpr::create(in_parent, expr->Iex.Mux0X.exprX))
{ }

VexExprMux0X::~VexExprMux0X(void)
{
	delete cond;
	delete expr0;
	delete exprX;
}

inline Value* flattenType(IRBuilder<> *builder, Value* v)
{
	if (isa<VectorType>(v->getType())) {
		return builder->CreateBitCast(v, IntegerType::get(
			getGlobalContext(),
			static_cast<const VectorType*>(
				v->getType())->getBitWidth()));
	}

	return v;
}

Value* VexExprMux0X::emit(void) const
{
	IRBuilder<>	*builder;
	Value		*cmp_val, *cmp_i8, *zero_val, *nonzero_val;
	PHINode		*pn;
	BasicBlock	*bb_zero, *bb_nonzero, *bb_merge, *bb_origin;
	BasicBlock	*bb_nz, *bb_z;	/* BB in case of nested MuX */

	builder = theGenLLVM->getBuilder();
	bb_origin = builder->GetInsertBlock();
	bb_zero = BasicBlock::Create(
		getGlobalContext(), "mux0X_zero",
		bb_origin->getParent());
	bb_nonzero = BasicBlock::Create(
		getGlobalContext(), "mux0X_nonzero",
		bb_origin->getParent());
	bb_merge = BasicBlock::Create(
		getGlobalContext(), "mux0X_merge",
		bb_origin->getParent());


	/* evaluate mux condition */
	builder->SetInsertPoint(bb_origin);
	cmp_val = cond->emit();
	cmp_i8 = builder->CreateICmpEQ(
		cmp_val,
		ConstantInt::get(
			getGlobalContext(),
			APInt(
				cmp_val->getType()->getPrimitiveSizeInBits(),
				0)));
	builder->CreateCondBr(cmp_i8, bb_zero, bb_nonzero);

	builder->SetInsertPoint(bb_nonzero);
	nonzero_val = exprX->emit();
	nonzero_val = flattenType(builder, nonzero_val);
	bb_nz = builder->GetInsertBlock();
	builder->CreateBr(bb_merge);

	builder->SetInsertPoint(bb_zero);
	zero_val = expr0->emit();
	zero_val = flattenType(builder, zero_val);
	bb_z = builder->GetInsertBlock();
	builder->CreateBr(bb_merge);


	/* phi node on the mux */
	builder->SetInsertPoint(bb_merge);
	pn = builder->CreatePHI(zero_val->getType(), 0, "mux0x_phi");

	pn->addIncoming(zero_val, bb_z);
	pn->addIncoming(nonzero_val, bb_nz);

	return pn;
}

void VexExprMux0X::print(std::ostream& os) const
{
	os << "Mux0X(";
	cond->print(os);
	os << ", ";
	expr0->print(os);
	os << ", ";
	exprX->print(os);
	os << ")";
}


Value* VexExprGet::emit(void) const
{ return theGenLLVM->readCtx(offset, ty); }
