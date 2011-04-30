#ifndef VEXSTMT_H
#define VEXSTMT_H

#include <stdint.h>
#include <iostream>

extern "C" {
#include <valgrind/libvex.h>
#include <valgrind/libvex_ir.h>
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
	virtual ~VexStmtIMark() {}
	virtual void emit(void) const { /* ignore */ }
	virtual void print(std::ostream& os) const;
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
	VexStmtPutI(VexSB* in_parent, const IRStmt* in_stmt)
	 : VexStmt(in_parent, in_stmt) {}
	virtual ~VexStmtPutI() {}
	virtual void print(std::ostream& os) const;
private:
#if 0
IRRegArray* descr; /* Part of guest state treated as circular */
IRExpr*     ix;    /* Variable part of index into array */
Int         bias;  /* Constant offset part of index into array */
IRExpr*     data;  /* The value to write */
#endif
};

class VexStmtWrTmp : public VexStmt
{
public:
	VexStmtWrTmp(VexSB* in_parent, const IRStmt* in_stmt);
	virtual ~VexStmtWrTmp();
	virtual void emit(void) const;
	virtual void print(std::ostream& os) const;
private:
	unsigned int	tmp_reg;
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
	VexStmtCAS(VexSB* in_parent, const IRStmt* in_stmt)
	 : VexStmt(in_parent, in_stmt) {}
	virtual ~VexStmtCAS() {}
	virtual void print(std::ostream& os) const;
private:
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
	VexStmtDirty(VexSB* in_parent, const IRStmt* in_stmt)
	 : VexStmt(in_parent, in_stmt) {}
	virtual ~VexStmtDirty() {}
	virtual void print(std::ostream& os) const;
private:
};
class VexStmtMBE : public VexStmt
{
public:
	VexStmtMBE(VexSB* in_parent, const IRStmt* in_stmt)
	 : VexStmt(in_parent, in_stmt) {}
	virtual ~VexStmtMBE() {}
	virtual void print(std::ostream& os) const;
private:
};

class VexStmtExit : public VexStmt
{
public:
	VexStmtExit(VexSB* in_parent, const IRStmt* in_stmt)
	 : VexStmt(in_parent, in_stmt) {}
	virtual ~VexStmtExit() {}
	virtual void print(std::ostream& os) const;
private:
};

#endif
