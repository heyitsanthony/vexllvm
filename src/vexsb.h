#ifndef VEXSB_H
#define VEXSB_H

#include <vector>
#include <stdint.h>
#include <iostream>

extern "C" {
#include <valgrind/libvex.h>
#include <valgrind/libvex_ir.h>
}

#include "collection.h"
#include "guestmem.h"

class VexStmt;
class VexStmtIMark;
class VexExpr;

namespace llvm
{
	class Value;
	class Function;
	class Type;
}

class VexSB
{
public:
	/* may be requesting to translate a bad IRSB; return NULL */
	static VexSB* create(guest_ptr guest_addr, const IRSB* in_irsb);

	/* empty VSB for building vex stmts manually and then load()ing them */
	VexSB(guest_ptr guest_addr, unsigned int num_regs, IRType* in_types);

	// takes ownership
	void load(
		std::vector<VexStmt*>& stmts, IRJumpKind irjk,
		VexExpr* next);

	virtual ~VexSB(void);
	unsigned int getNumRegs(void) const { return reg_c; }
	unsigned int getNumStmts(void) const { return stmt_c; }
	void setRegValue(unsigned int reg_idx, llvm::Value* v);
	llvm::Value* getRegValue(unsigned int reg_idx) const;
	const llvm::Type* getRegType(unsigned int reg_idx) const;
	llvm::Function* emit(const char* f_name = "vexsb_f");
	void print(std::ostream& os) const;
	void printRegisters(std::ostream& os) const;
	static unsigned int getTypeBitWidth(IRType ty);
	static const char* getTypeStr(IRType ty);
	guest_ptr getJmp(void) const;
	guest_ptr getEndAddr(void) const;
	/* hopefully VEX only puts one syscall in each block... */
	bool isSyscall(void) const {
		return (jump_kind == Ijk_Sys_syscall ||
			jump_kind == Ijk_Sys_int128);
	}
	guest_ptr getGuestAddr(void) const { return guest_addr; }
	unsigned int getSize(void) const
	{
		return getEndAddr().o - getGuestAddr();
	}

	std::vector<InstExtent>	getInstExtents(void) const;

protected:
	VexSB(guest_ptr guess_addr, const IRSB* in_irsb);
private:
	void loadInstructions(const IRSB* irsb);
	void loadJump(IRJumpKind, VexExpr*);

	VexStmt* loadNextInstruction(const IRStmt* stmt);

	IRJumpKind		jump_kind;
	VexExpr			*jump_expr;
	guest_ptr		guest_addr;
	unsigned int		reg_c;
	unsigned int		stmt_c;
	VexStmtIMark		*last_imark;
	PtrList<VexStmt>	stmts;
	llvm::Value		**values;
	IRType			*types;
};

#endif
