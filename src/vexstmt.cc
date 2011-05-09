#include <stdio.h>
#include "guestcpustate.h"
#include "genllvm.h"
#include "vexexpr.h"
#include "vexsb.h"

#include "vexstmt.h"

void VexStmtNoOp::print(std::ostream& os) const { os << "NoOp"; }

void VexStmtIMark::print(std::ostream& os) const
{
	os <<	"IMark: Addr=" << (void*)guest_addr <<
		". OpLen= " << guest_op_len;
}
VexStmtIMark::VexStmtIMark(
	VexSB* in_parent, const IRStmt* in_stmt)
: VexStmt(in_parent, in_stmt),
  guest_addr(in_stmt->Ist.IMark.addr),
  guest_op_len(in_stmt->Ist.IMark.len)
{}

void VexStmtAbiHint::print(std::ostream& os) const { os << "AbiHint"; }

VexStmtPut::VexStmtPut(VexSB* in_parent, const IRStmt* in_stmt)
: VexStmt(in_parent, in_stmt),
  offset(in_stmt->Ist.Put.offset),
  data_expr(VexExpr::create(this, in_stmt->Ist.Put.data))
{
	assert (data_expr != NULL && "Bad data expr");
}
VexStmtPut::~VexStmtPut(void) { delete data_expr;}

void VexStmtPut::emit(void) const
{
	llvm::Value	*out_v;
	out_v = data_expr->emit();
	theGenLLVM->writeCtx(offset, out_v);
}

void VexStmtPut::print(std::ostream& os) const
{
	os << "Put(" << offset << ") <- ";
	data_expr->print(os);
}


void VexStmtPutI::print(std::ostream& os) const { os << "PutI"; }

VexStmtWrTmp::VexStmtWrTmp(
	VexSB* in_parent, const IRStmt* in_stmt)
: VexStmt(in_parent, in_stmt),
  tmp_reg(in_stmt->Ist.WrTmp.tmp),
  expr(VexExpr::create(this, in_stmt->Ist.WrTmp.data))
{}
VexStmtWrTmp::~VexStmtWrTmp(void) { delete expr; }

void VexStmtWrTmp::emit(void) const
{
	parent->setRegValue(tmp_reg, expr->emit());
}

void VexStmtWrTmp::print(std::ostream& os) const
{
	os << "WrTmp: t" << tmp_reg << " <- ";
	expr->print(os);
}

VexStmtStore::VexStmtStore(VexSB* in_parent, const IRStmt* in_stmt)
: VexStmt(in_parent, in_stmt),
  isLE((in_stmt->Ist.Store.end == Iend_LE)),
  addr_expr(VexExpr::create(this, in_stmt->Ist.Store.addr)),
  data_expr(VexExpr::create(this, in_stmt->Ist.Store.data))
{}

VexStmtStore::~VexStmtStore(void)
{
	delete addr_expr;
	delete data_expr;
}

void VexStmtStore::emit(void) const
{
	llvm::Value *addr_v, *data_v;

	data_v = data_expr->emit();
	addr_v = addr_expr->emit();
	theGenLLVM->store(addr_v, data_v);
}

void VexStmtStore::print(std::ostream& os) const
{
	os << "Store: (";
	addr_expr->print(os);
	os << ") <- ";
	data_expr->print(os);
}

void VexStmtCAS::print(std::ostream& os) const { os << "CAS"; }
void VexStmtLLSC::print(std::ostream& os) const { os << "LLSC"; }
void VexStmtDirty::print(std::ostream& os) const { os << "Dirty"; }
void VexStmtMBE::print(std::ostream& os) const { os << "MBE"; }


VexStmtExit::VexStmtExit(VexSB* in_parent, const IRStmt* in_stmt)
: VexStmt(in_parent, in_stmt),
 guard(VexExpr::create(this, in_stmt->Ist.Exit.guard)),
 jk(in_stmt->Ist.Exit.jk)
{
	switch (jk) {
	case Ijk_Boring: exit_type = (uint8_t)GE_IGNORE; break;
	case Ijk_SigSEGV: exit_type = (uint8_t)GE_SIGSEGV; break;
	case Ijk_EmWarn: exit_type = (uint8_t)GE_EMWARN; break;
	default:
		ppIRJumpKind(jk);
		assert (0 == 1 && "Unexpected Boring JumpKind");
	}

	switch(in_stmt->Ist.Exit.dst->tag) {
	#define CONST_TAGOP(x) case Ico_##x :		\
	dst = in_stmt->Ist.Exit.dst->Ico.x; return;	\

	CONST_TAGOP(U1);
	CONST_TAGOP(U8);
	CONST_TAGOP(U16);
	CONST_TAGOP(U32);
	CONST_TAGOP(U64);
	CONST_TAGOP(F32i);
	CONST_TAGOP(F64i);
//	CONST_TAGOP(V128); TODO
	default: fprintf(stderr, "UNSUPPORTED CONSTANT TYPE\n");
	}
}

VexStmtExit::~VexStmtExit()
{
	delete guard;
}

void VexStmtExit::emit(void) const
{
	llvm::IRBuilder<>	*builder;
	llvm::Value		*cmp_val;
	llvm::BasicBlock	*bb_then, *bb_else, *bb_origin;

	builder = theGenLLVM->getBuilder();
	bb_origin = builder->GetInsertBlock();
	bb_then = llvm::BasicBlock::Create(
		llvm::getGlobalContext(), "exit_then",
		bb_origin->getParent());
	bb_else = llvm::BasicBlock::Create(
		llvm::getGlobalContext(), "exit_else",
		bb_origin->getParent());

	/* evaluate guard condition */
	builder->SetInsertPoint(bb_origin);
	cmp_val = guard->emit();
	builder->CreateCondBr(cmp_val, bb_then, bb_else);

	/* guard condition return, leave this place */
	/* XXX for calls we're going to need some more info */
	builder->SetInsertPoint(bb_then);

	if (jk != Ijk_Boring) {	
		/* special exits set exit type */
		theGenLLVM->setExitType((GuestExitType)exit_type);
	}
	builder->CreateRet(
		llvm::ConstantInt::get(
			llvm::getGlobalContext(),
			llvm::APInt(64, dst)));

	/* continue on */
	builder->SetInsertPoint(bb_else);
}

void VexStmtExit::print(std::ostream& os) const
{
	os << "If (";
	guard->print(os);
	os << ") goto {...} " << dst;
}
