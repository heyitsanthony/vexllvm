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
}

class VexExpr
{
public:
	virtual ~VexExpr(void) {}
	virtual void print(std::ostream& os) const = 0;
	virtual llvm::Value* emit(void) const
	{ 
		assert (0 == 1 && "STUB");
		return NULL;
	}
	static VexExpr* create(VexStmt* in_parent, const IRExpr* expr);
protected:
	VexExpr(VexStmt* in_parent, const IRExpr* expr)
	: parent(in_parent) {}
private:
	const VexStmt* parent;
};

/* Read a guest register, at a fixed offset in the guest state. */
class VexExprGet : public VexExpr
{
public:
	VexExprGet(VexStmt* in_parent, const IRExpr* expr)
	:VexExpr(in_parent, expr), 
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
	: VexExpr(in_parent, expr) {}
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
	: VexExpr(in_parent, expr), tmp_reg(expr->Iex.RdTmp.tmp) {}
	virtual ~VexExprRdTmp(void) {}
	virtual void print(std::ostream& os) const;
private:
      /* The value held by a temporary.
         ppconst IRExpr output: t<tmp>, eg. t1
      */
      unsigned int tmp_reg;
};

class VexExprQop : public VexExpr
{
public:
	VexExprQop(VexStmt* in_parent, const IRExpr* expr)
	: VexExpr(in_parent, expr) {}
	virtual ~VexExprQop(void) {}
	virtual void print(std::ostream& os) const;
private:
      /* A quaternary operation.
         ppconst IRExpr output: <op>(<arg1>, <arg2>, <arg3>, <arg4>),
                      eg. MAddF64r32(t1, t2, t3, t4)
      */
      struct {
         IROp op;          /* op-code   */
         const IRExpr* arg1;     /* operand 1 */
         const IRExpr* arg2;     /* operand 2 */
         const IRExpr* arg3;     /* operand 3 */
         const IRExpr* arg4;     /* operand 4 */
      } Qop;
};

class VexExprTriop : public VexExpr
{
public:
	VexExprTriop(VexStmt* in_parent, const IRExpr* expr)
	: VexExpr(in_parent, expr) {}
	virtual ~VexExprTriop(void) {}
	virtual void print(std::ostream& os) const;
private:
      /* A ternary operation.
         ppconst IRExpr output: <op>(<arg1>, <arg2>, <arg3>),
                      eg. MulF64(1, 2.0, 3.0)
      */
      struct {
         IROp op;          /* op-code   */
         const IRExpr* arg1;     /* operand 1 */
         const IRExpr* arg2;     /* operand 2 */
         const IRExpr* arg3;     /* operand 3 */
      } Triop;
};

class VexExprBinop : public VexExpr
{
public:
	VexExprBinop(VexStmt* in_parent, const IRExpr* expr);
	virtual ~VexExprBinop(void);
	virtual void print(std::ostream& os) const;
private:
	IROp op;		/* op-code   */
	VexExpr* arg1;	/* operand 1 */
	VexExpr* arg2;	/* operand 2 */
};

class VexExprUnop : public VexExpr
{
public:
	VexExprUnop(VexStmt* in_parent, const IRExpr* expr);
	virtual ~VexExprUnop(void);
	virtual void print(std::ostream& os) const;
	virtual const char* getOpName(void) const = 0;
	static VexExprUnop* createUnop(VexStmt* in_parent, const IRExpr* expr);
protected:
	/* A unary operation. */
	IROp    op;		/* op-code */
	VexExpr	*arg_expr;	/* operand */
};

#define UNOP_CLASS(x)	\
class VexExprUnop##x : public VexExprUnop	\
{	\
public:	\
	VexExprUnop##x(VexStmt* in_parent, const IRExpr* expr)	\
	: VexExprUnop(in_parent, expr) {}	\
	virtual ~VexExprUnop##x() {}	\
	virtual const char* getOpName(void) const { return #x; }	\
private:	\
}

UNOP_CLASS(32Uto64);
UNOP_CLASS(64to32);

class VexExprLoad : public VexExpr
{
public:
	VexExprLoad(VexStmt* in_parent, const IRExpr* expr)
	: VexExpr(in_parent, expr) {}
	virtual ~VexExprLoad(void) {}
	virtual void print(std::ostream& os) const;
private:
      /* A load from memory -- a normal load, not a load-linked.
         Load-Linkeds (and Store-Conditionals) are instead represented
         by IRStmt.LLSC since Load-Linkeds have side effects and so
         are not semantically valid const IRExpr's.
         ppconst IRExpr output: LD<end>:<ty>(<addr>), eg. LDle:I32(t1)
      */
      struct {
         IREndness end;    /* Endian-ness of the load */
         IRType    ty;     /* Type of the loaded value */
         const IRExpr*   addr;   /* Address being loaded from */
      } Load;
};

class VexExprConst : public VexExpr
{
public:
	virtual ~VexExprConst(void) {}
	virtual void print(std::ostream& os) const = 0;
	static VexExprConst* createConst(
		VexStmt* in_parent, const IRExpr* expr);
protected:
	VexExprConst(VexStmt* in_parent, const IRExpr* expr)
	: VexExpr(in_parent, expr) {}
private:
      /* A constant-valued expression.
         ppconst IRExpr output: <con>, eg. 0x4:I32 */
};

#define CONST_CLASS(x,y)			\
class VexExprConst##x : public VexExprConst	\
{	\
public:	\
	VexExprConst##x(VexStmt* in_parent, const IRExpr* expr)	\
	: VexExprConst(in_parent, expr),	\
	x(expr->Iex.Const.con->Ico.x) {}	\
	void print(std::ostream& os) const { os << x << ":" #x; } \
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
CONST_CLASS(F32, float);
CONST_CLASS(F32i, uint32_t);

class VexExprCCall : public VexExpr
{
public:
	VexExprCCall(VexStmt* in_parent, const IRExpr* expr)
	: VexExpr(in_parent, expr) {}
	virtual ~VexExprCCall(void) {}
	virtual void print(std::ostream& os) const;
private:
      /* A call to a pure (no side-effects) helper C function. */
      struct {
         IRCallee* cee;    /* Function to call. */
         IRType    retty;  /* Type of return value. */
         const IRExpr**  args;   /* Vector of argument expressions. */
      }  CCall;
};

class VexExprMux0X : public VexExpr
{
public:
	VexExprMux0X(VexStmt* in_parent, const IRExpr* expr)
	: VexExpr(in_parent, expr) {}
	virtual ~VexExprMux0X(void) {}
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
