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
{}
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
void VexStmtExit::print(std::ostream& os) const { os << "Exit"; }
