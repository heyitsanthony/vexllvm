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

namespace llvm
{
	class Value;
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
	void emit(void);
	void print(std::ostream& os) const;
	void printRegisters(std::ostream& os) const;
	static unsigned int getTypeBitWidth(IRType ty);
	static const char* getTypeStr(IRType ty);
private:
	void loadBitWidths(const IRTypeEnv* tyenv);
	void loadInstructions(const IRSB* irsb);
	VexStmt* loadNextInstruction(const IRStmt* stmt);
	uint64_t		guest_addr;
	unsigned int		reg_c;
	unsigned int		*reg_bitwidth;
	unsigned int		stmt_c;
	PtrList<VexStmt>	stmts;
	llvm::Value		**values;
};

#endif
