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
protected:
	VexExpr(VexStmt* in_parent)
	: parent(in_parent) {}
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
	VexExprGetI(VexStmt* in_parent, const IRExpr* expr)
	: VexExpr(in_parent) {}
	virtual ~VexExprGetI(void) {}
	virtual void print(std::ostream& os) const;
private:

      struct {
         IRRegArray* descr; /* Part of guest state treated as circular */
         const IRExpr*     ix;    /* Variable part of index into array */
         Int         bias;  /* Constant offset part of index into array */
      } GetI;
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
	void print(std::ostream& os) const { os << x << ":" #x; } \
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
CONST_CLASS(F64i, uint64_t);
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

#endif
