#include "genllvm.h"

#include <stdio.h>
#include "vexsb.h"
#include "vexstmt.h"
#include "vexop.h"

#include "vexexpr.h"

VexExpr* VexExpr::create(VexStmt* in_parent, const IRExpr* expr)
{
	switch (expr->tag) {
#define EXPR_TAGOP(x) case Iex_##x : \
	return new VexExpr##x(in_parent, expr)
	EXPR_TAGOP(Get);
	EXPR_TAGOP(GetI);
	EXPR_TAGOP(RdTmp);
	case Iex_Qop:
	case Iex_Triop:
	case Iex_Binop:
	case Iex_Unop: 
	return VexExprNaryOp::createOp(in_parent, expr);
	EXPR_TAGOP(Load);
	case Iex_Const:
		return VexExprConst::createConst(in_parent, expr);

	EXPR_TAGOP(Mux0X);
	EXPR_TAGOP(CCall);
	case Iex_Binder:
	default:
		fprintf(stderr, "Expr: ??? %x\n", expr->tag);
	}
	return NULL;
}

VexExpr* VexExprNaryOp::createOp(VexStmt* in_parent, const IRExpr* expr)
{
	IROp	op = expr->Iex.Unop.op;

	/* known ops */
	switch (op) {
#define BINOP_TAGOP(x) case Iop_##x : \
return new VexExprBinop##x(in_parent, expr);
#define UNOP_TAGOP(x) case Iop_##x : \
return new VexExprUnop##x(in_parent, expr)
	BINOP_TAGOP(And64);
	BINOP_TAGOP(Add64);
	BINOP_TAGOP(Sub64);
	UNOP_TAGOP(32Uto64);
	UNOP_TAGOP(64to32);
	default:
		fprintf(stderr, "UNKNOWN OP %x\n", expr->Iex.Unop.op);
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
	CONST_TAGOP(F32);
	CONST_TAGOP(F32i);
	CONST_TAGOP(F64);
	CONST_TAGOP(F64i);
//	CONST_TAGOP(V128); TODO
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
EMIT_CONST_INT(F32i, 32)
EMIT_CONST_INT(F64i, 64)

llvm::Value* VexExprConstF32::emit(void) const {
	return llvm::ConstantFP::get(
		llvm::getGlobalContext(),
		llvm::APFloat(F32)); }

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
: VexExpr(in_parent, expr),
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
: VexExpr(in_parent, expr)
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
void VexExprCCall::print(std::ostream& os) const { os << "CCall"; }
void VexExprMux0X::print(std::ostream& os) const { os << "Mux0X"; }


llvm::Value* VexExprGet::emit(void) const
{
	/* XXX should we worry about doing coercion here? */
	return theGenLLVM->readCtx(offset);
}
