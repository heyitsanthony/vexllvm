#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "genllvm.h"

#include "Sugar.h"

#include "vexsb.h"
#include "vexstmt.h"
#include "vexexpr.h"
#include "guestcpustate.h"

using namespace llvm;

VexSB::VexSB(guest_ptr in_guest_addr, const IRSB* irsb)
:	guest_addr(in_guest_addr),
	reg_c(irsb->tyenv->types_used),
	stmt_c(irsb->stmts_used),
	last_imark(0)
{
	values = new Value*[reg_c];
	memset(values, 0, sizeof(Value*)*getNumRegs());
	types = new IRType[reg_c];
	memcpy(types, irsb->tyenv->types, reg_c * sizeof(IRType));

	loadInstructions(irsb);
	loadJump(irsb->jumpkind, VexExpr::create(stmts.back(), irsb->next));
}

VexSB* VexSB::create(guest_ptr guest_addr, const IRSB* in_irsb)
{
	if (in_irsb == NULL) return NULL;

	/* error decoding? likely jump to creepy place in memory */
	if (in_irsb->jumpkind == Ijk_NoDecode)
		return NULL;

	return new VexSB(guest_addr, in_irsb);
}

VexSB::~VexSB(void)
{
	delete [] values;
	delete jump_expr;
	delete [] types;
}

VexSB::VexSB(guest_ptr in_guest_addr, unsigned int num_regs, IRType* in_types)
: 	guest_addr(in_guest_addr),
	reg_c(num_regs),
	stmt_c(0),
	last_imark(0)
{
	values = new Value*[reg_c];
	memset(values, 0, sizeof(Value*)*getNumRegs());
	types = new IRType[reg_c];
	memcpy(types, in_types, reg_c * sizeof(IRType));
}

void VexSB::load(
	std::vector<VexStmt*>& in_stmts,
	IRJumpKind irjk,
	VexExpr* next_expr)
{
	assert (stmt_c == 0);

	stmt_c = in_stmts.size();
	foreach (it, in_stmts.begin(), in_stmts.end()) {
		VexStmt		*stmt;
		VexStmtIMark	*imark;

		stmt = *it;
		stmts.push_back(stmt);

		imark = dynamic_cast<VexStmtIMark*>(stmt);
		if (imark != NULL) last_imark = imark;
	}

	loadJump(irjk, next_expr);

	/* take it all away */
	in_stmts.clear();
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

void VexSB::print(std::ostream& os) const
{
	unsigned int i = 0;
	printRegisters(os);
	os << "\n-----irsb----";
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

		os <<	"t" << i << " ";

		v = values[i];
		if (v == NULL) continue;
		std::string s = v->getNameStr();
		os << s;
		v->dump();
		os << "\n";
	}
}

llvm::Function* VexSB::emit(const char* fname)
{
	llvm::Function* cur_f;
	llvm::Value*	ret_v;

	memset(values, 0, sizeof(Value*)*getNumRegs());

	theGenLLVM->beginBB(fname);

	/* instructions */
	foreach (it, stmts.begin(), stmts.end()) (*it)->emit();

	/* compute goto */
	ret_v = jump_expr->emit();

	/* record exit type */
	switch(jump_kind) {
	case Ijk_Boring:
	case Ijk_NoRedir:
	case Ijk_ClientReq:
		/* nothing, boring */
		break;
	case Ijk_Call:
		theGenLLVM->setExitType(GE_CALL);
		break;
	case Ijk_Ret:
		theGenLLVM->setExitType(GE_RETURN);
		break;
	case Ijk_Sys_syscall:
	case Ijk_Sys_int128:
		theGenLLVM->setExitType(GE_SYSCALL);
		break;
	case Ijk_SigTRAP:
		theGenLLVM->setExitType(GE_SIGTRAP);
		break;
	default:
		fprintf(stderr, "UNKNOWN JUMP TYPE %x\n", jump_kind);
		assert(0 == 1 && "BAD JUMP");
	}

	/* return goto */
	cur_f = theGenLLVM->endBB(ret_v);

	return cur_f;
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

std::vector<InstExtent>	VexSB::getInstExtents(void) const
{
	std::vector<InstExtent>	ret;

	foreach (it, stmts.begin(), stmts.end()) {
		VexStmt		*vs = *it;
		VexStmtIMark	*im;

		im = dynamic_cast<VexStmtIMark*>(vs);
		if (im == NULL)
			continue;

		ret.push_back(
			InstExtent(im->getAddr(), (uint8_t)im->getLen()));
	}

	return ret;
}

void VexSB::loadInstructions(const IRSB* irsb)
{
	ppIRSB(const_cast<IRSB*>(irsb));
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

llvm::Type* VexSB::getRegType(unsigned int reg_idx) const
{ return theGenLLVM->vexTy2LLVM(types[reg_idx]); }

void VexSB::loadJump(IRJumpKind jk, VexExpr* blk_next)
{
	jump_kind = jk;

	switch(jk) {
	case Ijk_Call:
	case Ijk_Ret:
	case Ijk_Boring:
	case Ijk_NoRedir:
	case Ijk_Sys_syscall:
	case Ijk_Sys_int128:
	case Ijk_SigTRAP:
	case Ijk_ClientReq:
		jump_expr = blk_next;
		break;
	default:
		fprintf(stderr, "UNKNOWN JUMP TYPE %x\n", jk);
		assert(0 == 1 && "BAD JUMP");
	}
}

guest_ptr VexSB::getJmp(void) const
{
	VexExprConst	*cexpr;

	cexpr = dynamic_cast<VexExprConst*>(jump_expr);
	if (cexpr == NULL) return guest_ptr(0);

	return guest_ptr(cexpr->toValue());
}

guest_ptr VexSB::getEndAddr(void) const
{
	assert (last_imark != NULL);
	return last_imark->getAddr() + last_imark->getLen();
}
