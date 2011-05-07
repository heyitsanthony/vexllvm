#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "genllvm.h"

#include "Sugar.h"

#include "vexsb.h"
#include "vexstmt.h"
#include "vexexpr.h"

using namespace llvm;

VexSB::VexSB(uint64_t in_guest_addr, const IRSB* irsb)
:	guest_addr(in_guest_addr),
	reg_c(irsb->tyenv->types_used),
	stmt_c(irsb->stmts_used)
{
	values = new Value*[reg_c];
	memset(values, 0, sizeof(Value*)*getNumRegs());
	reg_bitwidth = new unsigned int[reg_c];
	memset(reg_bitwidth, 0, reg_c*sizeof(unsigned int));

	loadBitWidths(irsb->tyenv);
	loadInstructions(irsb);
	loadJump(irsb);
}

VexSB::~VexSB(void)
{
	delete [] values;
	delete [] reg_bitwidth;
}

unsigned int VexSB::getTypeBitWidth(IRType ty)
{
	switch(ty) {
	case Ity_I1:	return 1;
	case Ity_I8:	return 8;
	case Ity_I16:	return 16;
	case Ity_I32:	return 32;
	case Ity_I64:	return 64;
	case Ity_I128:	return 128;
	case Ity_F32:	return 32;
	case Ity_F64:	return 64;
	case Ity_V128:	return 128;
	default:
		fprintf(stderr, "Unknown type %x in VexSB.\n", ty);
	}
	return 0;
}

const char* VexSB::getTypeStr(IRType ty)
{
	switch(ty) {
	case Ity_I1:	return "I1";
	case Ity_I8:	return "I8";
	case Ity_I16:	return "I16";
	case Ity_I32:	return "I32";
	case Ity_I64:	return "I64";
	case Ity_I128:	return "I128";
	case Ity_F32:	return "F32";
	case Ity_F64:	return "F64";
	case Ity_V128:	return "V128";
	default:
		fprintf(stderr, "Unknown type %x in VexSB.\n", ty);
	}
	return 0;
}

void VexSB::loadBitWidths(const IRTypeEnv* tyenv)
{
	for (unsigned int i = 0; i < reg_c; i++)
		reg_bitwidth[i] = getTypeBitWidth(tyenv->types[i]);
}

void VexSB::print(std::ostream& os) const
{
	unsigned int i = 0;
	foreach (it, stmts.begin(), stmts.end()) {
		os << "[" << i++ << "]: ";
		(*it)->print(os);
		os << "\n";
	}
}

void VexSB::printRegisters(std::ostream& os) const
{
	os << "Registers:\n";
	for (unsigned int i = 0; i < getNumRegs(); i++) {
		const Value*	v;

		os <<	"t" << i << ":I" << 
			getRegBitWidth(i) << "\n";

		v = values[i];
		if (v == NULL) continue;
		std::string s = v->getNameStr();
		os << s << "\n";
	}
}

llvm::Function* VexSB::emit(const char* fname)
{
	llvm::Function* cur_f;
	llvm::Value*	ret_v;

	theGenLLVM->beginBB(fname);
	/* instructions */
	foreach (it, stmts.begin(), stmts.end()) (*it)->emit();
	/* compute goto */
	ret_v = jump_expr->emit();
	/* return goto */
	cur_f = theGenLLVM->endBB(ret_v);

	return cur_f;
}

unsigned int VexSB::getRegBitWidth(unsigned int reg_idx) const
{
	assert (reg_idx < getNumRegs());
	return reg_bitwidth[reg_idx];
}

VexStmt* VexSB::loadNextInstruction(const IRStmt* stmt)
{
	VexStmt	*vs;

#define TAGOP(x) case Ist_##x : vs = new VexStmt##x(this, stmt); break

	switch (stmt->tag) {
	TAGOP(NoOp);
	TAGOP(IMark);
	TAGOP(AbiHint);
	TAGOP(Put);
	TAGOP(PutI);
	TAGOP(WrTmp);
	TAGOP(Store);
	TAGOP(Exit);
	TAGOP(CAS);
	TAGOP(LLSC);
	TAGOP(Dirty);
	TAGOP(MBE);
	default: 
	printf("???\n");
	assert (0 && "SIMPLE HUMAN");
	break;
	}
	return vs;
}

void VexSB::loadInstructions(const IRSB* irsb)
{
	ppIRSB (const_cast<IRSB*>(irsb));

	for (unsigned int i = 0; i < getNumStmts(); i++) {
		VexStmt		*stmt;
		VexStmtIMark	*imark;

		stmt = loadNextInstruction(irsb->stmts[i]);
		stmts.push_back(stmt);

		imark = dynamic_cast<VexStmtIMark*>(stmt);
		if (imark != NULL) last_imark = imark;
	}
}

void VexSB::setRegValue(unsigned int reg_idx, Value* v)
{
	assert (values[reg_idx] == NULL);
	values[reg_idx] = v;
}

Value* VexSB::getRegValue(unsigned int reg_idx) const
{
	assert (values[reg_idx] != NULL);
	return values[reg_idx];
}

void VexSB::loadJump(const IRSB* irsb)
{
	jump_kind = irsb->jumpkind;

//	ppIRJumpKind (jump_kind);
//	printf("=JUMPKIND\n");

	switch(irsb->jumpkind) {
	case Ijk_Call:
	case Ijk_Ret:
	case Ijk_Boring:
	case Ijk_NoRedir:
	case Ijk_Sys_syscall:
		jump_expr = VexExpr::create(stmts.back(), irsb->next);
		break;
	default:
		fprintf(stderr, "UNKNOWN JUMP TYPE %x\n", irsb->jumpkind);
		assert(0 == 1 && "BAD JUMP");
	}
}

uint64_t VexSB::getJmp(void) const
{
	VexExprConst	*cexpr;

	cexpr = dynamic_cast<VexExprConst*>(jump_expr);
	if (cexpr == NULL) return 0;

	return cexpr->toValue();
}

uint64_t VexSB::getEndAddr(void) const
{
	assert (last_imark != NULL);
	return last_imark->getAddr() + last_imark->getLen();
}
