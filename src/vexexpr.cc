#include "genllvm.h"
#include "Sugar.h"
#include <stdio.h>
#include "vexsb.h"
#include "vexstmt.h"
#include "vexop.h"
#include "vexhelpers.h"

#include "vexexpr.h"

unsigned int VexExpr::vex_expr_op_count[VEX_MAX_OP];

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

	op = expr->Iex.Unop.op;

	/* bump expression operation count */
	op_idx = op - Iop_INVALID;
	assert (op_idx < VEX_MAX_OP && "Op is larger than VEX_MAX_OP - OFLOW");
	VexExpr::vex_expr_op_count[op_idx]++;

	/* known ops */
	switch (op) {
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

	BINOP_TAGOP(CmpEQ8x16);
	BINOP_TAGOP(CmpGT8Sx16);
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

	UNOP_TAGOP(Sqrt64F0x2);
	BINOP_TAGOP(Min64F0x2);
	BINOP_TAGOP(Max32F0x4);
	BINOP_TAGOP(CmpLT32F0x4);
	BINOP_TAGOP(CmpEQ64F0x2);

	BINOP_TAGOP(SetV128lo64);
	BINOP_TAGOP(SetV128lo32);

	BINOP_TAGOP(InterleaveLO8x16);
	BINOP_TAGOP(InterleaveLO64x2);
	BINOP_TAGOP(InterleaveHI64x2);

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
	BINOP_TAGOP(Sub8x16);

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

	UNOP_TAGOP(1Uto8);
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
	UNOP_TAGOP(16Sto32);
	UNOP_TAGOP(32Uto64);
	UNOP_TAGOP(32Sto64);
	UNOP_TAGOP(32to8);
	UNOP_TAGOP(32to16);
	UNOP_TAGOP(64to32);
	UNOP_TAGOP(64to1);
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
	UNOP_TAGOP(32HIto16);
	UNOP_TAGOP(64HIto32);
	UNOP_TAGOP(F32toF64);
	BINOP_TAGOP(F64toF32);
	UNOP_TAGOP(I32StoF64);

	UNOP_TAGOP(ReinterpF64asI64);
	UNOP_TAGOP(ReinterpI64asF64);
	UNOP_TAGOP(ReinterpF32asI32);
	UNOP_TAGOP(ReinterpI32asF32);

	BINOP_TAGOP(I64StoF64);
//	BINOP_TAGOP(I64UtoF64); 3.7.0
	BINOP_TAGOP(F64toI32S);
	BINOP_TAGOP(F64toI32U);
	BINOP_TAGOP(F64toI64S);


	UNOP_TAGOP(Ctz64);
	UNOP_TAGOP(Clz64);

	BINOP_TAGOP(Shl8x8);
	BINOP_TAGOP(Shr8x8);
	BINOP_TAGOP(Sar8x8);
	BINOP_TAGOP(Sal8x8);

	BINOP_TAGOP(Shl16x4);
	BINOP_TAGOP(Shr16x4);
	BINOP_TAGOP(Sar16x4);
	BINOP_TAGOP(Sal16x4);

	BINOP_TAGOP(Shl32x2);
	BINOP_TAGOP(Shr32x2);
	BINOP_TAGOP(Sar32x2);
	BINOP_TAGOP(Sal32x2);

	BINOP_TAGOP(SarN8x8);
	BINOP_TAGOP(ShlN8x8);
	BINOP_TAGOP(ShrN8x8);

	BINOP_TAGOP(ShlN16x4);
	BINOP_TAGOP(ShrN16x4);
	BINOP_TAGOP(SarN16x4);

	BINOP_TAGOP(ShlN32x2);
	BINOP_TAGOP(ShrN32x2);
	BINOP_TAGOP(SarN32x2);

	BINOP_TAGOP(Perm8x8);
	BINOP_TAGOP(Perm8x16);

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

#define EMIT_CONST_INT(x,y)	\
llvm::Value* VexExprConst##x::emit(void) const { 	\
	return llvm::ConstantInt::get(			\
		llvm::getGlobalContext(), llvm::APInt(y, x)); }
EMIT_CONST_INT(U1, 1)
EMIT_CONST_INT(U8, 8)
EMIT_CONST_INT(U16, 16)
EMIT_CONST_INT(U32, 32)
EMIT_CONST_INT(U64, 64)
//EMIT_CONST_INT(F32i, 32) 3.7.0
EMIT_CONST_INT(F64i, 64)

llvm::Value* VexExprConstV128::emit(void) const
{
	llvm::VectorType* v128ty = llvm::VectorType::get(
		llvm::Type::getInt16Ty(
			llvm::getGlobalContext()),
		8);
	std::vector<llvm::Constant*> splat(
		8,
		llvm::ConstantInt::get(
			llvm::getGlobalContext(),
			llvm::APInt(16, V128)));

	return theGenLLVM->to16x8i(llvm::ConstantVector::get(v128ty, splat));
}

// 3.7.0
#if 0
llvm::Value* VexExprConstF32::emit(void) const {
	return llvm::ConstantFP::get(
		llvm::getGlobalContext(),
		llvm::APFloat(F32)); }
#endif

llvm::Value* VexExprConstF64::emit(void) const {
	return llvm::ConstantFP::get(
		llvm::getGlobalContext(),
		llvm::APFloat(F64)); }

void VexExprGet::print(std::ostream& os) const
{
	os << "GET(" << offset << "):" << VexSB::getTypeStr(ty);
}

void VexExprGetI::print(std::ostream& os) const { os << "GetI"; }

void VexExprRdTmp::print(std::ostream& os) const
{
	os << "RdTmp(t" << tmp_reg << ")";
}

llvm::Value* VexExprRdTmp::emit(void) const
{
	const VexStmt	*stmt = getParent();
	const VexSB	*sb = stmt->getParent();
	return sb->getRegValue(tmp_reg);
}

VexExprNaryOp::VexExprNaryOp(
	VexStmt* in_parent, const IRExpr* expr, unsigned int in_n_ops)
: VexExpr(in_parent),
  op(expr->Iex.Unop.op),
  n_ops(in_n_ops)
{
	/* HACK HACK cough */
	const IRExpr**	ir_exprs;
	ir_exprs = (const IRExpr**)((void*)(&expr->Iex.Unop.arg));

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

llvm::Value* VexExprLoad::emit(void) const
{
	llvm::Value	*addr_expr;
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


llvm::Value* VexExprCCall::emit(void) const
{
	std::vector<llvm::Value*>	args_v;

	foreach (it, args.begin(), args.end())
		args_v.push_back((*it)->emit());

	return theGenLLVM->getBuilder()->CreateCall(
		func, args_v.begin(), args_v.end());
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

llvm::Value* VexExprMux0X::emit(void) const
{
	llvm::IRBuilder<>	*builder;
	llvm::Value		*cmp_val, *cmp_i8, *zero_val, *nonzero_val;
	llvm::PHINode		*pn;
	llvm::BasicBlock	*bb_zero, *bb_nonzero, *bb_merge, *bb_origin;
	llvm::BasicBlock	*bb_nz, *bb_z;	/* BB in case of nested MuX */

	builder = theGenLLVM->getBuilder();
	bb_origin = builder->GetInsertBlock();
	bb_zero = llvm::BasicBlock::Create(
		llvm::getGlobalContext(), "mux0X_zero",
		bb_origin->getParent());
	bb_nonzero = llvm::BasicBlock::Create(
		llvm::getGlobalContext(), "mux0X_nonzero",
		bb_origin->getParent());
	bb_merge = llvm::BasicBlock::Create(
		llvm::getGlobalContext(), "mux0X_merge",
		bb_origin->getParent());


	/* evaluate mux condition */
	builder->SetInsertPoint(bb_origin);
	cmp_val = cond->emit();
	cmp_i8 = builder->CreateICmpEQ(
		cmp_val,
		llvm::ConstantInt::get(
			llvm::getGlobalContext(),
			llvm::APInt(
				cmp_val->getType()->getPrimitiveSizeInBits(),
				0)));
	builder->CreateCondBr(cmp_i8, bb_zero, bb_nonzero);

	builder->SetInsertPoint(bb_nonzero);
	nonzero_val = exprX->emit();
	bb_nz = builder->GetInsertBlock();
	builder->CreateBr(bb_merge);

	builder->SetInsertPoint(bb_zero);
	zero_val = expr0->emit();
	bb_z = builder->GetInsertBlock();
	builder->CreateBr(bb_merge);


	/* phi node on the mux */
	builder->SetInsertPoint(bb_merge);
	pn = builder->CreatePHI(zero_val->getType(), "mux0x_phi");
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


llvm::Value* VexExprGet::emit(void) const
{
	return theGenLLVM->readCtx(offset, ty);
}
