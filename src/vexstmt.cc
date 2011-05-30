#include <stdio.h>
#include "Sugar.h"

#include "guestcpustate.h"
#include "genllvm.h"
#include "vexhelpers.h"
#include "vexexpr.h"
#include "vexsb.h"

#include "vexstmt.h"

using namespace llvm;

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
	Value	*out_v;
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
	Value *addr_v, *data_v;

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

VexStmtCAS::VexStmtCAS(VexSB* in_parent, const IRStmt* in_stmt)
 :	VexStmt(in_parent, in_stmt),
 	oldVal_tmp(in_stmt->Ist.CAS.details->oldLo),
 	addr(VexExpr::create(this, in_stmt->Ist.CAS.details->addr)),
	expected_val(VexExpr::create(this, in_stmt->Ist.CAS.details->expdLo)),
	new_val(VexExpr::create(this, in_stmt->Ist.CAS.details->dataLo))
{
	assert (in_stmt->Ist.CAS.details->expdHi == NULL &&
		"Only supporting single width CAS");
}

VexStmtCAS::~VexStmtCAS()
{
	delete addr;
	delete expected_val;
	delete new_val;
}

void VexStmtCAS::emit(void) const
{
	Value		*v_addr, *v_addr_load, *v_expected, *v_new, *v_cmp;
	BasicBlock	*bb_then, *bb_merge, *bb_origin;
	IRBuilder<>	*builder;

// If addr contains the same value as expected_val, then new_val is
// written there, else there is no write.  In both cases, the
// original value at .addr is copied into .oldLo.
 	builder = theGenLLVM->getBuilder();
	v_addr = addr->emit();
	v_expected = expected_val->emit();
	v_addr_load = theGenLLVM->load(v_addr, v_expected->getType());

	bb_origin = builder->GetInsertBlock();
	bb_then = BasicBlock::Create(
		getGlobalContext(),
		"cas_match_expect",
		bb_origin->getParent());
	bb_merge = BasicBlock::Create(
		getGlobalContext(),
		"cas_merge",
		bb_origin->getParent());

	/* compare value at address with expected value*/
	builder->SetInsertPoint(bb_origin);
	v_cmp = builder->CreateICmpEQ(v_expected, v_addr_load);
	builder->CreateCondBr(v_cmp, bb_then, bb_merge);

	builder->SetInsertPoint(bb_then);
	/* match, write new value to address */
	v_new = new_val->emit();
	theGenLLVM->store(v_addr, v_new);
	builder->CreateBr(bb_merge);

	builder->SetInsertPoint(bb_merge);
	parent->setRegValue(oldVal_tmp, v_addr_load);
}

void VexStmtCAS::print(std::ostream& os) const
{
	os << "CAS(*";
	addr->print(os);
	os << " == ";
	expected_val->print(os);
	os << " => ";
	new_val->print(os);
	os << ")" << " tmp=t" << oldVal_tmp;
}

void VexStmtLLSC::print(std::ostream& os) const { os << "LLSC"; }

VexStmtDirty::VexStmtDirty(VexSB* in_parent, const IRStmt* in_stmt)
  :	VexStmt(in_parent, in_stmt),
  	guard(VexExpr::create(this, in_stmt->Ist.Dirty.details->guard)),
  	needs_state_ptr(in_stmt->Ist.Dirty.details->needsBBP),
	state_base_ptr(NULL),
	tmp_reg(in_stmt->Ist.Dirty.details->tmp)
{
	const char*	func_name;
	IRExpr**	in_args;

	/* load function */
	func_name = in_stmt->Ist.Dirty.details->cee->name;
	func = theVexHelpers->getHelper(func_name);
	if (func == NULL) {
		std::cerr << "Could not find dirty function \"" <<
			func_name << "\". Bye" << std::endl;
		assert (func != NULL && "Could not find dirty function");
	}

	/* put cpu state ptr on arg stack */
	if (needs_state_ptr) {
		const Type	*ptrty;	
		state_base_ptr = theGenLLVM->getCtxBase();
		/* result is an i8*, pull the correct ptr type func */
		ptrty = ((func->arg_begin()))->getType();
		state_base_ptr = theGenLLVM->getBuilder()->CreateBitCast(
			state_base_ptr, ptrty);
	}

	in_args = in_stmt->Ist.Dirty.details->args;
	for (unsigned int i = 0; in_args[i]; i++)
		args.push_back(VexExpr::create(this, in_args[i]));
}

VexStmtDirty::~VexStmtDirty()
{
	delete guard;
}

void VexStmtDirty::emit(void) const
{
	IRBuilder<>		*builder;
	Value			*v_cmp, *v_call;
	BasicBlock		*bb_then, *bb_merge, *bb_origin;
	std::vector<Value*>	args_v;

	builder = theGenLLVM->getBuilder();
	bb_origin = builder->GetInsertBlock();
	bb_then = BasicBlock::Create(
		getGlobalContext(), "dirty_then",
		bb_origin->getParent());
	bb_merge = BasicBlock::Create(
		getGlobalContext(), "dirty_merge",
		bb_origin->getParent());

	/* evaluate guard condition */
	builder->SetInsertPoint(bb_origin);
	v_cmp = guard->emit();
	builder->CreateCondBr(v_cmp, bb_then, bb_merge);

	/* guard condition OK, make drty call */
	builder->SetInsertPoint(bb_then);
	if (needs_state_ptr) args_v.push_back(state_base_ptr);
	foreach (it, args.begin(), args.end()) 
		args_v.push_back((*it)->emit());
	v_call = builder->CreateCall(func, args_v.begin(), args_v.end());

	/* sometimes dirty calls don't set temporaries */
	if (tmp_reg != -1)
		parent->setRegValue(tmp_reg, v_call);

	builder->CreateBr(bb_merge);

	builder->SetInsertPoint(bb_merge);
}

void VexStmtDirty::print(std::ostream& os) const
{
	os << "DirtyCall(" << func->getNameStr() << ")"; 
}


void VexStmtMBE::emit(void) const
{
	/* only one type of memory bus event-- fence */
	theGenLLVM->memFence();
}

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
	CONST_TAGOP(F64i);
	default: fprintf(stderr, "UNSUPPORTED CONSTANT TYPE\n");
	}
}

VexStmtExit::~VexStmtExit()
{
	delete guard;
}

void VexStmtExit::emit(void) const
{
	IRBuilder<>	*builder;
	Value		*v_cmp;
	BasicBlock	*bb_then, *bb_else, *bb_origin;

	builder = theGenLLVM->getBuilder();
	bb_origin = builder->GetInsertBlock();
	bb_then = BasicBlock::Create(
		getGlobalContext(), "exit_then",
		bb_origin->getParent());
	bb_else = BasicBlock::Create(
		getGlobalContext(), "exit_else",
		bb_origin->getParent());

	/* evaluate guard condition */
	builder->SetInsertPoint(bb_origin);
	v_cmp = guard->emit();
	builder->CreateCondBr(v_cmp, bb_then, bb_else);

	/* guard condition return, leave this place */
	/* XXX for calls we're going to need some more info */
	builder->SetInsertPoint(bb_then);

	if (jk != Ijk_Boring) {	
		/* special exits set exit type */
		theGenLLVM->setExitType((GuestExitType)exit_type);
	}
	builder->CreateRet(
		ConstantInt::get(
			getGlobalContext(),
			APInt(64, dst)));

	/* continue on */
	builder->SetInsertPoint(bb_else);
}

void VexStmtExit::print(std::ostream& os) const
{
	os << "If (";
	guard->print(os);
	os << ") goto {...} " << (void*)dst;
}
