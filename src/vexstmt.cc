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
	os <<	"IMark: Addr=" << (void*)guest_addr.o <<
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

VexStmtPutI::VexStmtPutI(VexSB* in_parent, const IRStmt* in_stmt)
: VexStmt(in_parent, in_stmt),
  base(in_stmt->Ist.PutI.details->descr->base),
  len(in_stmt->Ist.PutI.details->descr->nElems),
  ix_expr(VexExpr::create(this, in_stmt->Ist.PutI.details->ix)),
  bias(in_stmt->Ist.PutI.details->bias),
  data_expr(VexExpr::create(this, in_stmt->Ist.PutI.details->data))
{
	assert (ix_expr != NULL && "Bad ix expr");
	assert (data_expr != NULL && "Bad data expr");
}

void VexStmtPutI::emit(void) const
{
	Value	*out_v, *ix_v;
	out_v = data_expr->emit();
	ix_v = ix_expr->emit();
	theGenLLVM->writeCtx(base, bias, len, ix_v, out_v);
}


void VexStmtPutI::print(std::ostream& os) const {
	os << "PutI(" << base << ", "
		<< bias << ", ";
	ix_expr->print(os);
	os << ", " << len << ") <- ";
	data_expr->print(os);
}

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

VexStmtLLSC::VexStmtLLSC(VexSB* in_parent, const IRStmt* in_stmt)
: VexStmt(in_parent, in_stmt)
, result(in_stmt->Ist.LLSC.result)
, addr_expr(VexExpr::create(this, in_stmt->Ist.LLSC.addr))
, data_expr(NULL)
{
	if(in_stmt->Ist.LLSC.storedata) {
		data_expr = VexExpr::create(this,
			in_stmt->Ist.LLSC.storedata);
	}
}
void VexStmtLLSC::emitLoad(void) const {
	parent->setRegValue(result,
		theGenLLVM->load(
			addr_expr->emit(),
			parent->getRegType(result)));
	theGenLLVM->markLinked();
}
void VexStmtLLSC::emitStore(void) const {
	Value		*v_addr, *v_linked;
	BasicBlock	*bb_linked, *bb_unlinked, *bb_origin;
	IRBuilder<>	*builder;

	builder = theGenLLVM->getBuilder();
	v_linked = theGenLLVM->getLinked();

	bb_origin = builder->GetInsertBlock();
	bb_linked = BasicBlock::Create(
		getGlobalContext(),
		"linked",
		bb_origin->getParent());
	bb_unlinked = BasicBlock::Create(
		getGlobalContext(),
		"unlinked",
		bb_origin->getParent());

	builder->SetInsertPoint(bb_origin);
	builder->CreateCondBr(v_linked, bb_linked, bb_unlinked);

	/* we're linked still, so write it */
	builder->SetInsertPoint(bb_linked);
	v_addr = addr_expr->emit();
	theGenLLVM->store(v_addr, data_expr->emit());
	builder->CreateBr(bb_unlinked);

	/* not linked, skip it */
	builder->SetInsertPoint(bb_unlinked);

	parent->setRegValue(result, v_linked);
}
void VexStmtLLSC::emit(void) const {
	if(data_expr) {
		emitStore();
	} else {
		emitLoad();
	}
}
VexStmtLLSC::~VexStmtLLSC() {
	delete addr_expr;
	delete data_expr;
}
void VexStmtLLSC::print(std::ostream& os) const {
	if(data_expr) {
		os << "LinkedStore: (";
		addr_expr->print(os);
		os << ") <- ";
		data_expr->print(os);
	} else {
		os << "LoadLinked: t" << result << " <- (";
		addr_expr->print(os);
		os << ")";
	}
}

VexStmtCAS::VexStmtCAS(VexSB* in_parent, const IRStmt* in_stmt)
 :	VexStmt(in_parent, in_stmt),
 	oldVal_tmp(in_stmt->Ist.CAS.details->oldLo),
	oldVal_tmpHI(in_stmt->Ist.CAS.details->oldHi),
 	addr(VexExpr::create(this, in_stmt->Ist.CAS.details->addr)),
	expected_val(VexExpr::create(this, in_stmt->Ist.CAS.details->expdLo)),
	expected_valHI(
		(in_stmt->Ist.CAS.details->expdHi)
		? VexExpr::create(this, in_stmt->Ist.CAS.details->expdHi)
		: NULL),
	new_val(VexExpr::create(this, in_stmt->Ist.CAS.details->dataLo)),
	new_valHI(
		(in_stmt->Ist.CAS.details->expdHi)
		? VexExpr::create(this, in_stmt->Ist.CAS.details->dataHi)
		: NULL)
{}

VexStmtCAS::~VexStmtCAS()
{
	delete addr;
	delete expected_val;
	delete new_val;
}

void VexStmtCAS::emit(void) const
{
	Value		*v_addr[2], *v_cmp,
			*v_addr_load[2], *v_expected[2], *v_new[2];
	BasicBlock	*bb_then, *bb_merge, *bb_origin;
	IRBuilder<>	*builder;

// If addr contains the same value as expected_val, then new_val is
// written there, else there is no write.  In both cases, the
// original value at .addr is copied into .oldLo.
 	builder = theGenLLVM->getBuilder();
	v_addr[0] = addr->emit();
	v_addr[1] = NULL;

	v_expected[0] = expected_val->emit();
	v_expected[1] = NULL;

	v_addr_load[0] = theGenLLVM->load(v_addr[0], v_expected[0]->getType());
	v_addr_load[1] = NULL;

	if (expected_valHI) {
		unsigned a_w, t_w;

		a_w = v_addr[0]->getType()->getPrimitiveSizeInBits();
		/* widths of both parts are equal */
		t_w = v_expected[0]->getType()->getPrimitiveSizeInBits();

		v_expected[1] = expected_valHI->emit();
		v_addr[1] = builder->CreateAdd(v_addr[0],
				ConstantInt::get(
					getGlobalContext(),
					APInt(a_w, t_w/8)));
		v_addr_load[1] = theGenLLVM->load(
			v_addr[1], v_expected[0]->getType());
	}


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

	v_cmp = builder->CreateICmpEQ(v_expected[0], v_addr_load[0]);
	if (v_expected[1]) {
		v_cmp = builder->CreateICmpEQ(
			v_cmp,
			builder->CreateICmpEQ(
				v_expected[1],
				v_addr_load[1]));
	}
	builder->CreateCondBr(v_cmp, bb_then, bb_merge);

	builder->SetInsertPoint(bb_then);
	/* match, write new value to address */
	v_new[0] = new_val->emit();
	theGenLLVM->store(v_addr[0], v_new[0]);
	if (v_addr[1]) {
		v_new[1] = new_val->emit();
		theGenLLVM->store(v_addr[1], v_new[1]);
	}

	builder->CreateBr(bb_merge);

	builder->SetInsertPoint(bb_merge);

	parent->setRegValue(oldVal_tmp, v_addr_load[0]);
	if (v_addr[1]) {
		parent->setRegValue(oldVal_tmpHI, v_addr_load[1]);
	}
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

VexStmtDirty::VexStmtDirty(VexSB* in_parent, const IRStmt* in_stmt)
  :	VexStmt(in_parent, in_stmt),
  	guard(VexExpr::create(this, in_stmt->Ist.Dirty.details->guard)),
  	needs_state_ptr(in_stmt->Ist.Dirty.details->needsBBP),
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

	if (needs_state_ptr) {
		args_v.push_back(
			theGenLLVM->getBuilder()->CreateBitCast(
				theGenLLVM->getCtxBase(),
				func->arg_begin()->getType()));
	}

	foreach (it, args.begin(), args.end())
		args_v.push_back((*it)->emit());
	v_call = builder->CreateCall(
		func, llvm::ArrayRef<llvm::Value*>(args_v));

	/* sometimes dirty calls don't set temporaries */
	if (tmp_reg != -1) {
		/* vex dirty calls don't necessarily return the correct type,
		   e.g. in the case of FLDENV on AMD64 it will return a
		   VexEmWarn enumeration which the compiler assigned to an
		   int, yet the VEX IR wants it to be 64-bit.  In C there
		   isn't a good way to control the storage type for an enum,
		   so I elected to just do an autocast here rather then modify
		   vex to strip out the type or change the generated ir. */
		Value* result = v_call;
		Type* ret_type = func->getReturnType();
		Type* tmp_type = parent->getRegType(tmp_reg);
		unsigned ret_bits = ret_type->getPrimitiveSizeInBits();
		unsigned tmp_bits = tmp_type->getPrimitiveSizeInBits();
		Type* compat_type = IntegerType::get(
			getGlobalContext(), ret_bits);

		if(ret_bits < tmp_bits) {
			result = builder->CreateBitCast(result, compat_type);
			result = builder->CreateZExt(result, tmp_type);
		} else if (ret_bits > tmp_bits) {
			result = builder->CreateBitCast(result, compat_type);
			result = builder->CreateTrunc(result, tmp_type);
		}
		parent->setRegValue(tmp_reg, result);
	}

	builder->CreateBr(bb_merge);
	builder->SetInsertPoint(bb_merge);
}

void VexStmtDirty::print(std::ostream& os) const
{
	os << "DirtyCall(" << func->getName().str() << ")";
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
	/* arm has some conditional calls occasionally */
	case Ijk_Call: exit_type = (uint8_t)GE_CALL; break;
	case Ijk_Ret: exit_type = (uint8_t)GE_RETURN; break;
	case Ijk_Sys_int128: exit_type = (uint8_t)GE_INT; break;
	case Ijk_Sys_syscall: exit_type = (uint8_t)GE_SYSCALL; break;
	case Ijk_Sys_sysenter: exit_type = (uint8_t)GE_SYSCALL; break;
	case Ijk_MapFail: /* shows up with ldt/gdt stuff-- fake as sigsegv */
	case Ijk_SigSEGV: exit_type = (uint8_t)GE_SIGSEGV; break;
	case Ijk_EmWarn: exit_type = (uint8_t)GE_EMWARN; break;
	/* it is allowed to have one of these show up in arm code. it has
	   something to do with the BLX instruction which calls a function
	   and switches to THUMB mode */
	case Ijk_NoDecode: exit_type = (uint8_t)GE_EMWARN; break;
	default:
		ppIRJumpKind(jk);
		assert (0 == 1 && "Unexpected Boring JumpKind");
	}

	switch(in_stmt->Ist.Exit.dst->tag) {
	#define CONST_TAGOP(x) case Ico_##x :			\
	dst = guest_ptr(in_stmt->Ist.Exit.dst->Ico.x); return;	\

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
	os << ") goto {...} " << (void*)dst.o;
}
