#ifndef VEXSTMT_H
#define VEXSTMT_H

#include <stdint.h>
#include <iostream>

#include "collection.h"

extern "C" {
#include <valgrind/libvex.h>
#include <valgrind/libvex_ir.h>
}

namespace llvm
{
class Function;
}

class VexSB;
class VexExpr;

class VexStmt
{
public:
	virtual ~VexStmt(void) {}
	virtual void emit(void) const { assert (0 == 1 && "STUB"); }
	virtual void print(std::ostream& os) const = 0;
	const VexSB* getParent(void) const { return parent; }
protected:
	VexStmt(VexSB* in_parent, const IRStmt* in_stmt)
		: parent(in_parent) {}
	VexSB* 	parent;
};

class VexStmtNoOp : public VexStmt
{
public:
	VexStmtNoOp(VexSB* in_parent, const IRStmt* in_stmt)
	 : VexStmt(in_parent, in_stmt) {}
	virtual ~VexStmtNoOp() {}
	virtual void emit(void) const { /* ignore */ }
	virtual void print(std::ostream& os) const;
private:
};

class VexStmtIMark : public VexStmt
{
public:
	VexStmtIMark(VexSB* in_parent, const IRStmt* in_stmt);
	VexStmtIMark(VexSB* in_parent, uint64_t in_ga, unsigned int in_len)
	: VexStmt(in_parent, NULL), guest_addr(in_ga), guest_op_len(in_len) {}
	virtual ~VexStmtIMark() {}
	virtual void emit(void) const { /* ignore */ }
	virtual void print(std::ostream& os) const;
	uint64_t getAddr(void) const { return guest_addr; }
	unsigned int getLen(void) const { return guest_op_len; }
private:
	uint64_t	guest_addr;
	unsigned int	guest_op_len;
};

class VexStmtAbiHint : public VexStmt
{
public:
	VexStmtAbiHint(VexSB* in_parent, const IRStmt* in_stmt)
	 : VexStmt(in_parent, in_stmt) {}
	virtual ~VexStmtAbiHint() {}
	virtual void emit(void) const { /* ignore */ }
	virtual void print(std::ostream& os) const; 
private:
};

class VexStmtPut : public VexStmt
{
public:
	VexStmtPut(VexSB* in_parent, const IRStmt* in_stmt);
	VexStmtPut(VexSB* in_parent, int in_offset, VexExpr* in_data_expr)
	: VexStmt(in_parent, NULL), offset(in_offset), data_expr(in_data_expr)
	{}
	virtual ~VexStmtPut();
	virtual void emit(void) const;
	virtual void print(std::ostream& os) const;
private:
	int	offset; /* Offset into the guest state */
	VexExpr	*data_expr; /* The value to write */
};

/* Write a guest register, at a non-fixed offset in the guest
 state.  See the comment for GetI expressions for more
 information. */
class VexStmtPutI : public VexStmt
{
public:
	VexStmtPutI(VexSB* in_parent, const IRStmt* in_stmt);
	virtual ~VexStmtPutI() {}
	virtual void emit(void) const;
	virtual void print(std::ostream& os) const;
private:
	int base;
	int len;
	VexExpr*     ix_expr;    /* Variable part of index into array */
	int         bias;  /* Constant offset part of index into array */
	VexExpr*     data_expr;  /* The value to write */
};

class VexStmtWrTmp : public VexStmt
{
public:
	VexStmtWrTmp(VexSB* in_parent, const IRStmt* in_stmt);
	virtual ~VexStmtWrTmp();
	virtual void emit(void) const;
	virtual void print(std::ostream& os) const;
private:
	int		tmp_reg;
	VexExpr		*expr;
};

class VexStmtStore : public VexStmt
{
public:
	VexStmtStore(VexSB* in_parent, const IRStmt* in_stmt);
	virtual ~VexStmtStore();
	virtual void emit(void) const;
	virtual void print(std::ostream& os) const;
private:
	bool	isLE;
	VexExpr	*addr_expr;
	VexExpr	*data_expr;
};

class VexStmtCAS : public VexStmt
{
public:
	VexStmtCAS(VexSB* in_parent, const IRStmt* in_stmt);
	virtual ~VexStmtCAS();
	virtual void emit(void) const;
	virtual void print(std::ostream& os) const;
private:
	unsigned int oldVal_tmp;
	VexExpr	*addr;
	VexExpr	*expected_val;
	VexExpr	*new_val;
};

class VexStmtLLSC : public VexStmt
{
public:
	VexStmtLLSC(VexSB* in_parent, const IRStmt* in_stmt)
	 : VexStmt(in_parent, in_stmt) {}
	virtual ~VexStmtLLSC() {}
	virtual void print(std::ostream& os) const;
private:
};

class VexStmtDirty : public VexStmt
{
public:
	VexStmtDirty(VexSB* in_parent, const IRStmt* in_stmt);
	virtual ~VexStmtDirty();
	virtual void emit(void) const;
	virtual void print(std::ostream& os) const;
private:
	VexExpr			*guard;
	llvm::Function*		func;
	PtrList<VexExpr>	args;
	bool			needs_state_ptr;
	llvm::Value		*state_base_ptr;
	int			tmp_reg;	/* where to store */
};

class VexStmtMBE : public VexStmt
{
public:
	VexStmtMBE(VexSB* in_parent, const IRStmt* in_stmt)
	 : VexStmt(in_parent, in_stmt) {}
	virtual ~VexStmtMBE() {}
	virtual void emit(void) const;
	virtual void print(std::ostream& os) const;
private:
};

class VexStmtExit : public VexStmt
{
public:
	VexStmtExit(VexSB* in_parent, const IRStmt* in_stmt);
	virtual ~VexStmtExit();
	virtual void emit(void) const;
	virtual void print(std::ostream& os) const;
private:
/* IRStmt.Ist.Exit */
	VexExpr		*guard;	/* Conditional expression-- always 1-bit */
	uint8_t		exit_type;
	IRJumpKind	jk;	/* Jump kind */
	uint64_t	dst;	/* target constant */
};

#endif
