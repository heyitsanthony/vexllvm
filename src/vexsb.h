#ifndef VEXSB_H
#define VEXSB_H

#include <stdint.h>
#include <iostream>

extern "C" {
#include <valgrind/libvex.h>
#include <valgrind/libvex_ir.h>
}

#include "collection.h"

class VexStmt;
class VexStmtIMark;
class VexExpr;

namespace llvm
{
	class Value;
	class Function;
};

class VexSB
{
public:
	VexSB(uint64_t guess_addr, const IRSB* in_irsb);
	virtual ~VexSB(void);
	unsigned int getRegBitWidth(unsigned int reg_idx) const;
	unsigned int getNumRegs(void) const { return reg_c; }
	unsigned int getNumStmts(void) const { return stmt_c; }
	void setRegValue(unsigned int reg_idx, llvm::Value* v);
	llvm::Value* getRegValue(unsigned int reg_idx) const;
	llvm::Function* emit(const char* f_name = "vexsb_f");
	void print(std::ostream& os) const;
	void printRegisters(std::ostream& os) const;
	static unsigned int getTypeBitWidth(IRType ty);
	static const char* getTypeStr(IRType ty);
	uint64_t getJmp(void) const;
	uint64_t getEndAddr(void) const;
	bool fallsThrough(void) const;
private:
	void loadBitWidths(const IRTypeEnv* tyenv);
	void loadInstructions(const IRSB* irsb);
	void loadJump(const IRSB* irsb);

	VexStmt* loadNextInstruction(const IRStmt* stmt);

	IRJumpKind		jump_kind;
	VexExpr			*jump_expr;
	uint64_t		guest_addr;
	unsigned int		reg_c;
	unsigned int		*reg_bitwidth;
	unsigned int		stmt_c;
	VexStmtIMark		*last_imark;
	PtrList<VexStmt>	stmts;
	llvm::Value		**values;
};

#endif
