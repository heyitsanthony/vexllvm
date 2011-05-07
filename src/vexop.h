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
UNOP_CLASS(64to1);

BINOP_CLASS(Add8);
BINOP_CLASS(Add16);
BINOP_CLASS(Add32);
BINOP_CLASS(Add64);

BINOP_CLASS(And8);
BINOP_CLASS(And16);
BINOP_CLASS(And32);
BINOP_CLASS(And64);

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

BINOP_CLASS(Xor8);
BINOP_CLASS(Xor16);
BINOP_CLASS(Xor32);
BINOP_CLASS(Xor64);


BINOP_CLASS(CmpEQ64);
BINOP_CLASS(CmpLE64S);
BINOP_CLASS(CmpLE64U);

#endif
