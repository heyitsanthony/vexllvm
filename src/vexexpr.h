#ifndef VEXEXPR_H
#define VEXEXPR_H

#include <stdint.h>
#include <assert.h>
#include <iostream>

extern "C" {
#include <valgrind/libvex.h>
#include <valgrind/libvex_ir.h>
}

#include "collection.h"

class VexStmt;

namespace llvm
{
class Value;
class Function;
class Type;
}

class VexExpr
{
public:
	virtual ~VexExpr(void) {}
	virtual void print(std::ostream& os) const = 0;
	virtual llvm::Value* emit(void) const
	{
		std::cerr << "OOPS emitting: ";
		print(std::cerr);
		std::cerr << "\n";
		assert (0 == 1 && "STUB");
		return NULL;
	}
	static VexExpr* create(VexStmt* in_parent, const IRExpr* expr);
	const VexStmt* getParent(void) const { return parent; }
	static unsigned int getOpCount(IROp irop)
	{ return vex_expr_op_count[irop - Iop_INVALID];	}

	static unsigned getUnhandledCount(void) { return unhandled_expr_c; }

protected:
	VexExpr(VexStmt* in_parent)
	: parent(in_parent) {}
	/* no way more than 2K ops (at 713 now)! if more, assert thrown*/
	#define VEX_MAX_OP	2048
	static unsigned int vex_expr_op_count[VEX_MAX_OP];
	static unsigned unhandled_expr_c;
private:
	const VexStmt* parent;
};

/* Read a guest register, at a fixed offset in the guest state. */
class VexExprGet : public VexExpr
{
public:
	VexExprGet(VexStmt* in_parent, const IRExpr* expr)
	:VexExpr(in_parent),
	 offset(expr->Iex.Get.offset),
	 ty(expr->Iex.Get.ty) {}

	virtual ~VexExprGet(void) {}
	virtual llvm::Value* emit(void) const;
	virtual void print(std::ostream& os) const;
private:
	int	offset;
	IRType	ty;
};

/* Read a guest register, at a fixed offset in the guest state. */
class VexExprGetI : public VexExpr
{
public:
	VexExprGetI(VexStmt* in_parent, const IRExpr* expr);
	virtual ~VexExprGetI(void);
	virtual void print(std::ostream& os) const;
	virtual llvm::Value* emit(void) const;
private:
	int			base;
	int			len;
	VexExpr			*ix_expr; /* Variable part of index into array */
	int			bias;  /* Const offset part of index into array */
	llvm::Type*		elem_type;
};

class VexExprRdTmp : public VexExpr
{
public:
	VexExprRdTmp(VexStmt* in_parent, const IRExpr* expr)
	: VexExpr(in_parent), tmp_reg(expr->Iex.RdTmp.tmp) {}
	virtual ~VexExprRdTmp(void) {}
	virtual void print(std::ostream& os) const;
	virtual llvm::Value* emit(void) const;
private:
      /* The value held by a temporary.
         ppconst IRExpr output: t<tmp>, eg. t1
      */
      unsigned int tmp_reg;
};

#include "vexop.h"

class VexExprLoad : public VexExpr
{
public:
	VexExprLoad(VexStmt* in_parent, const IRExpr* expr);
	virtual ~VexExprLoad(void);
	virtual llvm::Value* emit(void) const;
	virtual void print(std::ostream& os) const;
private:
      /* A load from memory -- a normal load, not a load-linked.
         Load-Linkeds (and Store-Conditionals) are instead represented
         by IRStmt.LLSC since Load-Linkeds have side effects and so
         are not semantically valid const IRExpr's.
         ppconst IRExpr output: LD<end>:<ty>(<addr>), eg. LDle:I32(t1)
      */
	bool	little_endian;
	IRType	ty;	/* Type of the loaded value */
	VexExpr	*addr;
};

class VexExprConst : public VexExpr
{
public:
	virtual ~VexExprConst(void) {}
	virtual void print(std::ostream& os) const = 0;
	static VexExprConst* createConst(
		VexStmt* in_parent, const IRExpr* expr);
	virtual uint64_t toValue(void) const = 0;
protected:
	VexExprConst(VexStmt* in_parent)
	: VexExpr(in_parent) {}
private:
      /* A constant-valued expression.
         ppconst IRExpr output: <con>, eg. 0x4:I32 */
};

#define CONST_CLASS(x,y)			\
class VexExprConst##x : public VexExprConst	\
{	\
public:	\
	VexExprConst##x(VexStmt* in_parent, const IRExpr* expr)	\
	: VexExprConst(in_parent),		\
	x(expr->Iex.Const.con->Ico.x) {}	\
	VexExprConst##x(VexStmt* in_parent, y v)	\
	: VexExprConst(in_parent), x(v) {}		\
							\
	void print(std::ostream& os) const { 		\
		os << (void*)((intptr_t)x) << ":" #x; 	\
	} \
	llvm::Value* emit(void) const;		\
	virtual uint64_t toValue(void) const { return x; }	\
private:					\
	y	x;				\
}

CONST_CLASS(U1, bool);
CONST_CLASS(U8, uint8_t);
CONST_CLASS(U16, uint16_t);
CONST_CLASS(U32, uint32_t);
CONST_CLASS(U64, uint64_t);
CONST_CLASS(F64, double);
class VexExprConstF64i : public VexExprConst
{
public:
	VexExprConstF64i(VexStmt* in_parent, const IRExpr* expr)
	: VexExprConst(in_parent),
	x(expr->Iex.Const.con->Ico.F64i)
	{
		uint64_t	iex_f64i;
		iex_f64i = expr->Iex.Const.con->Ico.F64i;
		memcpy(&x, &iex_f64i, sizeof(iex_f64i));
	}
	VexExprConstF64i(VexStmt* in_parent, double v)
	: VexExprConst(in_parent), x(v) {}

	void print(std::ostream& os) const { os << x  << ":F64i"; }
	llvm::Value* emit(void) const;
	virtual uint64_t toValue(void) const { return x; }
private:
	double	x;
};
CONST_CLASS(V128, uint16_t);
/* wait for 3.7.0 */
//CONST_CLASS(F32, float);
//CONST_CLASS(F32i, uint32_t);

class VexExprCCall : public VexExpr
{
public:
	VexExprCCall(VexStmt* in_parent, const IRExpr* expr);
	virtual ~VexExprCCall(void) {}
	virtual llvm::Value* emit(void) const;
	virtual void print(std::ostream& os) const;
private:
	llvm::Function*		func;
	PtrList<VexExpr>	args;
};

class VexExprMux0X : public VexExpr
{
public:
	VexExprMux0X(VexStmt* in_parent, const IRExpr* expr);
	virtual ~VexExprMux0X(void);
	virtual llvm::Value* emit(void) const;
	virtual void print(std::ostream& os) const;
private:
      /* A ternary if-then-else operator.  It returns expr0 if cond is
         zero, exprX otherwise.  Note that it is STRICT, ie. both
         expr0 and exprX are evaluated in all cases.

         ppconst IRExpr output: Mux0X(<cond>,<expr0>,<exprX>),
                         eg. Mux0X(t6,t7,t8)
      */
      VexExpr	*cond;
      VexExpr	*expr0;	/* true expr */
      VexExpr	*exprX;	/* false expr */
};

class VexExprBBPTR : public VexExpr
{
public:
	VexExprBBPTR(VexStmt* in_parent, const IRExpr* expr);
	virtual ~VexExprBBPTR(void) {}
	virtual llvm::Value* emit(void) const;
	virtual void print(std::ostream& os) const;
private:
};

#endif
