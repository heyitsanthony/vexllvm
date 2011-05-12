#ifndef VEXOP_H
#define VEXOP_H

#include "vexexpr.h"



extern "C" {
#include <valgrind/libvex.h>
#include <valgrind/libvex_ir.h>
}

const char* getVexOpName(IROp op);

class VexExprNaryOp : public VexExpr
{
public:
	VexExprNaryOp(
		VexStmt* in_parent, const IRExpr* expr, unsigned int n_ops);
	VexExprNaryOp(
		VexStmt* in_parent, VexExpr** in_args, unsigned int in_n_ops)
	: 	VexExpr(in_parent),
		op((IROp)~0), n_ops(in_n_ops), args(in_args) {}
	virtual ~VexExprNaryOp(void);
	virtual void print(std::ostream& os) const;
	static VexExpr* createOp(VexStmt* p, const IRExpr* expr);
	virtual const char* getOpName(void) const { return "OP???"; }
protected:
	IROp		op;
	unsigned int	n_ops;
	VexExpr		**args;
};

#define DECL_NARG_OP(x,y)					\
class VexExpr##x##op : public VexExprNaryOp			\
{								\
public:								\
	VexExpr##x##op(VexStmt* p, const IRExpr* expr)		\
	: VexExprNaryOp(p, expr, y) {}				\
	VexExpr##x##op(VexStmt* p, VexExpr** args)		\
	: VexExprNaryOp(p, args, y) {}				\
	virtual ~VexExpr##x##op(void) {}			\
}

DECL_NARG_OP(Q, 4);
DECL_NARG_OP(Tri, 3);
DECL_NARG_OP(Bin, 2);
DECL_NARG_OP(Un, 1);

#define OP_CLASS(x,y)				\
class VexExpr##x##y : public VexExpr##x		\
{						\
public:						\
	VexExpr##x##y(VexStmt* in_parent, const IRExpr* expr)	\
	: VexExpr##x(in_parent, expr) {}	\
	VexExpr##x##y(VexStmt* in_parent, VexExpr** args)	\
	: VexExpr##x(in_parent, args) {}			\
	virtual ~VexExpr##x##y() {}		\
	virtual llvm::Value* emit(void) const;	\
	virtual const char* getOpName(void) const { return #y; }	\
private:	\
}

#define UNOP_CLASS(z)	OP_CLASS(Unop, z)
#define BINOP_CLASS(z)	OP_CLASS(Binop, z)

UNOP_CLASS(32Sto64);
UNOP_CLASS(32Uto64);
UNOP_CLASS(64to32);
UNOP_CLASS(64to8);
UNOP_CLASS(64to1);
UNOP_CLASS(64to16);
UNOP_CLASS(1Uto8);
UNOP_CLASS(1Uto64);
UNOP_CLASS(8Uto32);
UNOP_CLASS(8Sto32);
UNOP_CLASS(8Uto64);
UNOP_CLASS(16Uto64);
UNOP_CLASS(32UtoV128);
UNOP_CLASS(V128to64); // lo half
UNOP_CLASS(V128HIto64); // hi half
UNOP_CLASS(32HLto64);
UNOP_CLASS(64HLtoV128);
UNOP_CLASS(64HIto32);

UNOP_CLASS(Clz64);
UNOP_CLASS(Ctz64);

UNOP_CLASS(Not1);
UNOP_CLASS(Not8);
UNOP_CLASS(Not16);
UNOP_CLASS(Not32);
UNOP_CLASS(Not64);

BINOP_CLASS(Add8);
BINOP_CLASS(Add16);
BINOP_CLASS(Add32);
BINOP_CLASS(Add64);

BINOP_CLASS(And8);
BINOP_CLASS(And16);
BINOP_CLASS(And32);
BINOP_CLASS(And64);

BINOP_CLASS(InterleaveLO8x16);

BINOP_CLASS(Mul8);
BINOP_CLASS(Mul16);
BINOP_CLASS(Mul32);
BINOP_CLASS(Mul64);

BINOP_CLASS(Or8);
BINOP_CLASS(Or16);
BINOP_CLASS(Or32);
BINOP_CLASS(Or64);

BINOP_CLASS(Shl8);
BINOP_CLASS(Shl16);
BINOP_CLASS(Shl32);
BINOP_CLASS(Shl64);

BINOP_CLASS(Shr8);
BINOP_CLASS(Shr16);
BINOP_CLASS(Shr32);
BINOP_CLASS(Shr64);

BINOP_CLASS(Sub8);
BINOP_CLASS(Sub16);
BINOP_CLASS(Sub32);
BINOP_CLASS(Sub64);
BINOP_CLASS(Sub8x16);

BINOP_CLASS(Xor8);
BINOP_CLASS(Xor16);
BINOP_CLASS(Xor32);
BINOP_CLASS(Xor64);

BINOP_CLASS(CmpEQ8);
BINOP_CLASS(CmpEQ16);
BINOP_CLASS(CmpEQ32);
BINOP_CLASS(CmpEQ64);
BINOP_CLASS(CmpNE8);
BINOP_CLASS(CmpNE16);
BINOP_CLASS(CmpNE32);
BINOP_CLASS(CmpNE64);

BINOP_CLASS(CasCmpEQ8);
BINOP_CLASS(CasCmpEQ16);
BINOP_CLASS(CasCmpEQ32);
BINOP_CLASS(CasCmpEQ64);
BINOP_CLASS(CasCmpNE8);
BINOP_CLASS(CasCmpNE16);
BINOP_CLASS(CasCmpNE32);
BINOP_CLASS(CasCmpNE64);


BINOP_CLASS(CmpEQ8x16);
BINOP_CLASS(CmpLE64S);
BINOP_CLASS(CmpLE64U);
BINOP_CLASS(CmpLT64U);

#endif
