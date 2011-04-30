#include "genllvm.h"

#include <stdio.h>
#include "vexsb.h"
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
	EXPR_TAGOP(Qop);
	EXPR_TAGOP(Triop);
	EXPR_TAGOP(Binop);
	case Iex_Unop:
		return VexExprUnop::createUnop(in_parent, expr);
	EXPR_TAGOP(Load);
	case Iex_Const:
		return VexExprConst::createConst(in_parent, expr);

	EXPR_TAGOP(Mux0X);
	EXPR_TAGOP(CCall);
	case Iex_Binder:
	default:
		fprintf(stderr, "??? %d", expr->tag);
	}
	return NULL;
}

VexExprUnop* VexExprUnop::createUnop(
	VexStmt* in_parent, const IRExpr* expr)
{
	switch (expr->Iex.Unop.op) {
#define UNOP_TAGOP(x) case Iop_##x : \
return new VexExprUnop##x(in_parent, expr)
	UNOP_TAGOP(32Uto64);
	UNOP_TAGOP(64to32);
	default: break;
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
	CONST_TAGOP(V128);
	}
	return NULL;
}

void VexExprGet::print(std::ostream& os) const
{
	os <<	"Expr: GET(" << offset << 
		"):" << VexSB::getTypeStr(ty);
}

void VexExprGetI::print(std::ostream& os) const { os << "GetI"; }

void VexExprRdTmp::print(std::ostream& os) const
{
	os << "RdTmp(t" << tmp_reg << ")";
}

void VexExprQop::print(std::ostream& os) const { os << "Qop"; }
void VexExprTriop::print(std::ostream& os) const { os << "Triop"; }
VexExprBinop::VexExprBinop(VexStmt* in_parent, const IRExpr* expr)
: VexExpr(in_parent, expr),
 op(expr->Iex.Binop.op),
 arg1(VexExpr::create(in_parent, expr->Iex.Binop.arg1)),
 arg2(VexExpr::create(in_parent, expr->Iex.Binop.arg2))
{}
VexExprBinop::~VexExprBinop(void)
{
	delete arg1;
	delete arg2;
}

void VexExprBinop::print(std::ostream& os) const
{
	os << getVexOpName(op) << "(";
	arg1->print(os);
	os << ", ";
	arg2->print(os);
	os << ")";
}
	
VexExprUnop::VexExprUnop(VexStmt* in_parent, const IRExpr* expr)
: VexExpr(in_parent, expr),
  op(expr->Iex.Unop.op),
  arg_expr(VexExpr::create(in_parent, expr->Iex.Unop.arg))
{
}

VexExprUnop::~VexExprUnop(void) { delete arg_expr; }

void VexExprUnop::print(std::ostream& os) const
{
	os << getOpName() << "(";
	arg_expr->print(os);
	os << ")";
}

void VexExprLoad::print(std::ostream& os) const { os << "Load"; }
void VexExprCCall::print(std::ostream& os) const { os << "CCall"; }
void VexExprMux0X::print(std::ostream& os) const { os << "Mux0X"; }


llvm::Value* VexExprGet::emit(void) const
{
	/* XXX should we worry about doing coercion here? */
	return theGenLLVM->readCtx(offset);
}


