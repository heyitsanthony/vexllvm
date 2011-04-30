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

UNOP_CLASS(32Uto64);
UNOP_CLASS(64to32);
BINOP_CLASS(Add64);
BINOP_CLASS(Sub64);
BINOP_CLASS(And64);

#endif
