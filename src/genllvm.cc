#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>

#include <vector>
#include <iostream>
#include <sstream>

#include "guest.h"
#include "guestcpustate.h"

#include "genllvm.h"
#include "memlog.h"

std::unique_ptr<GenLLVM> theGenLLVM;

using namespace llvm;

unsigned int GenLLVM::mod_c = 0;

GenLLVM::GenLLVM(const Guest& gs, const char* name)
: guest(gs)
, guestCtxTy(NULL)
, funcTy(NULL)
, cur_guest_ctx(NULL)
, cur_memory_log(NULL)
, cur_f(NULL)
, entry_bb(NULL)
, log_last_store(getenv("VEXLLVM_LAST_STORE"))
, fake_vsys_reads(getenv("VEXLLVM_FAKE_VSYS") != NULL)
, use_reloc(true)
{
	builder = std::make_unique<IRBuilder<>>(getGlobalContext());
	assert (builder != NULL && "Could not create builder");

	takeModule(name);
	assert (mod != NULL && "Could not create mod");

	assert (guest.getCPUState() && "No CPU state set in Guest");
	mkFuncTy();
}

std::unique_ptr<Module> GenLLVM::takeModule(const char *name)
{
	std::unique_ptr<Module>	ret(std::move(mod));

	if (!name) {
		std::stringstream ss;
		ss << "genllvm_mod_" << mod_c++;
		mod = std::make_unique<Module>(ss.str(), getGlobalContext());
	} else {
		mod = std::make_unique<Module>(name, getGlobalContext());
	}
	assert (mod != NULL && "Could not create mod");

	// *any* data layout *should* work, but klee will horribly fail
	// if not LE and the vexllvm loads/stores would fail since it
	// ignores the ordering suffixes. Everything is broken; support
	// BE if/when it finally matters. Right now, force LE.
	//
	// Layout string taken from the vexops.bc file. Good as any other.
	mod->setDataLayout(
		"e-m:e-i64:64-f80:128-n8:16:32:64-S128");
//		"e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:"
//		"64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:"
//		"0:64-s0:64:64-f80:128:128-n8:16:32:64-S128");
	mod->setTargetTriple("x86_64-pc-linux-gnu");

	return ret;
}

void GenLLVM::beginBB(const char* name)
{
	assert (entry_bb == NULL && "nested beginBB");
	cur_f = Function::Create(
		funcTy,
		Function::ExternalLinkage,
		name,
		mod.get());

	entry_bb = BasicBlock::Create(getGlobalContext(), "entry", cur_f);
	builder->SetInsertPoint(entry_bb);

	Function::arg_iterator arg = cur_f->arg_begin();

	//arg->addAttr(Attributes(Attributes::NoAlias));
	cur_guest_ctx = arg++;

	if (log_last_store) {
		cur_memory_log = arg++;
		memlog_slot = 0;
	}
}

Function* GenLLVM::endBB(Value* retVal)
{
	Function	*ret_f;

	assert (entry_bb != NULL && "ending missing bb");

	/* FIXME. Should return next addr to jump to */
	builder->CreateRet(retVal);

	gepbyte_map.clear();
	ctxcast_map.clear();
	ret_f = cur_f;
	cur_f = NULL;
	entry_bb = NULL;
	cur_guest_ctx = NULL;
	cur_memory_log = NULL;
	return ret_f;
}

Type* GenLLVM::vexTy2LLVM(IRType ty)
{
	switch(ty) {
	case Ity_I1:	return IntegerType::get(getGlobalContext(), 1);
	case Ity_I8:	return IntegerType::get(getGlobalContext(), 8);
	case Ity_I16:	return IntegerType::get(getGlobalContext(), 16);
	case Ity_I32:	return IntegerType::get(getGlobalContext(), 32);
	case Ity_I64:	return IntegerType::get(getGlobalContext(), 64);
	case Ity_F32:	return Type::getFloatTy(getGlobalContext());
	case Ity_F64:	return Type::getDoubleTy(getGlobalContext());
//	case Ity_I128:	 TODO
	case Ity_V128:
		return VectorType::get(
			Type::getInt8Ty(getGlobalContext()),
			16);
	case Ity_V256:
		return VectorType::get(
			Type::getInt8Ty(getGlobalContext()),
			32);
	default:
		std::cout << "COULDN'T HANDLE " << ty << std::endl;
	}
	return NULL;
}

Value* GenLLVM::readCtx(unsigned int byteOff, IRType ty)
{
	Value	*ret;

	ret = getCtxByteGEP(byteOff, vexTy2LLVM(ty));
	ret = builder->CreateLoad(ret);

	return ret;
}

Value* GenLLVM::readCtx(
	unsigned int byteOff, int bias, int len,
	Value* ix,  Type* accessTy)
{
	Value		*ret, *addr;

	assert(byteOff % (accessTy->getPrimitiveSizeInBits() / 8) == 0);
	Type* offset_type = IntegerType::get(
		getGlobalContext(), sizeof(int)*8);
	Value* bias_v = ConstantInt::get(
		getGlobalContext(), APInt(sizeof(int)*8, bias));
	Value* len_v = ConstantInt::get(
		getGlobalContext(), APInt(sizeof(int)*8, len));
	Value* offset = builder->CreateAdd(
		builder->CreateBitCast(ix, offset_type),
		bias_v);
	Value* base_v = ConstantInt::get(
		getGlobalContext(),
		APInt(sizeof(unsigned int) * 8, byteOff /
		(accessTy->getPrimitiveSizeInBits() / 8)));
	offset = builder->CreateURem(offset, len_v);
	offset = builder->CreateAdd(offset, base_v);
	// XXX check for vector values?
	addr = getCtxTyGEP(offset, accessTy);
	ret = builder->CreateLoad(addr);

	return ret;
}

/* TODO: log these */
Value* GenLLVM::writeCtx(unsigned int byteOff, Value* v)
{
	Value		*ret, *addr;
	StoreInst	*si;

	addr = getCtxByteGEP(byteOff, v->getType());
	si = builder->CreateStore(v, addr);
	ret = si;
	return ret;
}

Value* GenLLVM::writeCtx(unsigned int byteOff, int bias, int len,
	Value* ix, Value* v)
{
	Value		*ret, *addr;
	StoreInst	*si;

	assert(byteOff % (v->getType()->getPrimitiveSizeInBits() / 8) == 0);
	Type* offset_type = IntegerType::get(
		getGlobalContext(), sizeof(int)*8);
	Value* bias_v = ConstantInt::get(
		getGlobalContext(), APInt(sizeof(int)*8, bias));
	Value* len_v = ConstantInt::get(
		getGlobalContext(), APInt(sizeof(int)*8, len));
	Value* offset = builder->CreateAdd(
		builder->CreateBitCast(ix, offset_type),
		bias_v);
	Value* base_v = ConstantInt::get(
		getGlobalContext(),
		APInt(sizeof(unsigned int) * 8, byteOff /
		(v->getType()->getPrimitiveSizeInBits() / 8)));
	offset = builder->CreateURem(offset, len_v);
	offset = builder->CreateAdd(offset, base_v);
	addr = getCtxTyGEP(offset, v->getType());
	si = builder->CreateStore(v, addr);
	ret = si;
	return ret;
}

Value* GenLLVM::getCtxByteGEP(unsigned int byteOff, Type* accessTy)
{
	unsigned int	tyBytes;
	Value		*gep;
	std::pair<unsigned, unsigned> key;
	gepbyte_map_t::const_iterator it;
	bool		found;

	tyBytes = accessTy->getPrimitiveSizeInBits()/8;
	assert (tyBytes && "Access type is 0 bytes???");

	key.first = byteOff;
	key.second = tyBytes;
	it = gepbyte_map.find(key);

	found = (it != gepbyte_map.end());

	if (	found &&
		entry_bb == builder->GetInsertBlock() &&
		cast<PointerType>(it->second->getType())->getElementType()
			 == accessTy)
	{
		return it->second;
	}

	gep = getCtxTyGEP(
		ConstantInt::get(
			getGlobalContext(),
			APInt(32, (byteOff/tyBytes))),
		accessTy);

	if (found == false)
		gepbyte_map.insert(std::make_pair(key, gep));

	return gep;
}

/* NOTE: offset is in units of accessTy! */
Value* GenLLVM::getCtxTyGEP(Value* off, Type* accessTy)
{
	ctxcast_map_t::const_iterator	cc_it;
	Type		*ptrTy;
	Value		*addr_ptr, *ret; /* XXX assuming access are aligned */
	const char	*gep_name;
	unsigned int	access_bytes;

	ptrTy = PointerType::get(accessTy, 0);
	cc_it = ctxcast_map.find(ptrTy);
	if (entry_bb != builder->GetInsertBlock()) {
		addr_ptr = builder->CreateBitCast(
			cur_guest_ctx, ptrTy, "regCtxPtr");
	} else if (cc_it == ctxcast_map.end()) {
		addr_ptr = builder->CreateBitCast(
			cur_guest_ctx, ptrTy, "regCtxPtr");
		ctxcast_map[ptrTy] = addr_ptr;
	} else {
		addr_ptr = cc_it->second;
	}

	gep_name = NULL;
	access_bytes = (accessTy->getPrimitiveSizeInBits()/8);
	if (isa<ConstantInt>(off)) {
		ConstantInt*	c_off = dyn_cast<ConstantInt>(off);
		uint64_t	off_u64;

		off_u64 = c_off->getZExtValue();
		gep_name = guest.getCPUState()->off2Name(
			access_bytes  * off_u64);
	}

	if (gep_name == NULL) gep_name = "unkCtxPtr";

	ret = builder->CreateGEP(addr_ptr, off, gep_name);
	return ret;
}

void GenLLVM::store(Value* addr_v, Value* data_v)
{
	Type		*ptrTy;
	Value		*addr_ptr;
	StoreInst	*si;

	if (cur_memory_log) {
		MemLog::recordStore(
			cur_memory_log, addr_v, data_v, memlog_slot);
		memlog_slot++;
	}

	ptrTy = PointerType::get(data_v->getType(), 0);
#ifdef __amd64__
	if(guest.getMem()->is32Bit()) {
		addr_v = builder->CreateZExt(addr_v, IntegerType::get(
			getGlobalContext(), sizeof(void*)*8));
	}
#endif
	if (guest.getMem()->getBase() && use_reloc) {
		addr_v = builder->CreateAdd(addr_v,
			ConstantInt::get(getGlobalContext(),
				APInt(sizeof(intptr_t)*8,
				(uintptr_t)guest.getMem()->getBase())));
	}
	addr_ptr = builder->CreateIntToPtr(addr_v, ptrTy, "storePtr");
	si = builder->CreateStore(data_v, addr_ptr);
	si->setAlignment(8);

}

Value* GenLLVM::load(Value* addr_v, Type* ty)
{
	Type		*ptrTy;
	Value		*addr_ptr;
	LoadInst	*loadInst;

	ptrTy = PointerType::get(ty, 0);
#ifdef __amd64__
	if(guest.getMem()->is32Bit()) {
		addr_v = builder->CreateZExt(addr_v, IntegerType::get(
			getGlobalContext(), sizeof(void*)*8));
	}

#endif
	/* XXX this is the worst hack but it's necessary for xchk until
	 * we get vsyspage stuff disabled programmatically -AJR */
	ConstantInt	*addr_ci;
	if (	fake_vsys_reads &&
		(addr_ci = dyn_cast<ConstantInt>(addr_v)) &&
		addr_ci->getBitWidth() <= 64)
	{
		const void	*sys_addr;
		sys_addr = guest.getMem()->getSysHostAddr(
			guest_ptr(addr_ci->getLimitedValue()));
		if (sys_addr != NULL) {
			unsigned int	ty_sz;
			uint64_t	out_v;

			ty_sz = ty->getPrimitiveSizeInBits();
			assert (ty_sz > 0 && ty_sz <= 64);
			memcpy(&out_v, sys_addr, ty_sz / 8);

			return ConstantInt::get(
				getGlobalContext(),
				APInt(ty_sz, out_v));
		}
	}


	if (guest.getMem()->getBase() && use_reloc) {
		addr_v = builder->CreateAdd(addr_v,
			ConstantInt::get(getGlobalContext(),
				APInt(sizeof(intptr_t)*8,
				(uintptr_t)guest.getMem()->getBase())));
	}
	addr_ptr = builder->CreateIntToPtr(addr_v, ptrTy, "loadPtr");
	loadInst = builder->CreateLoad(addr_ptr);
	loadInst->setAlignment(8);
	return loadInst;
}

Value* GenLLVM::load(Value* addr_v, IRType vex_type)
{
	return load(addr_v, vexTy2LLVM(vex_type));
}

void GenLLVM::markLinked() {
	/* DEATH: we need to set linked...
	   also how to do this in a processor indepent way, because
	   this is really an ARM problem */
	return;
}

Value* GenLLVM::getLinked() {
	return ConstantInt::get(getGlobalContext(), APInt(1, 1));
}

void GenLLVM::setExitType(uint8_t exit_type)
{
	writeCtx(
		guest.getCPUState()->getExitTypeOffset(),
		ConstantInt::get(
			getGlobalContext(),
			APInt(8, exit_type)));
}

/* llvm-ized VexSB functions take form of
 * guestaddr_t f(guest*) {  ...bullshit...; return ctrl_xfer_addr; } */
void GenLLVM::mkFuncTy(void)
{
	std::vector<Type*>	f_args;

	f_args.push_back(PointerType::get(getGuestTy(), 0));
	if(log_last_store) {
		f_args.push_back(PointerType::get(MemLog::getType(), 0));
	}

	funcTy = FunctionType::get(builder->getInt64Ty(), f_args, false);
}

Value* GenLLVM::to16x8i(Value* v) const
{
	return builder->CreateBitCast(
		v,
		VectorType::get(
			Type::getInt8Ty(getGlobalContext()), 16));
}

void GenLLVM::memFence(void) { builder->CreateFence(SequentiallyConsistent); }

Type* GenLLVM::getGuestTy(void)
{
	if (guestCtxTy)
		return guestCtxTy;

	const struct guest_ctx_field* f(guest.getCPUState()->getFields());
	std::vector<Type*>	types;
	Type			*i8ty, *i16ty, *i32ty,
				*i64ty, *i128ty, *i256ty;
	LLVMContext		&gctx(getGlobalContext());


	i8ty = Type::getInt8Ty(gctx);
	i16ty = Type::getInt16Ty(gctx);
	i32ty = Type::getInt32Ty(gctx);
	i64ty = Type::getInt64Ty(gctx);
	i128ty = VectorType::get(i16ty, 8);
	i256ty = VectorType::get(i16ty, 16);

	/* add all fields to types vector from structure */
	for (unsigned i = 0; f[i].f_len != 0; i++) {
		Type*	t;

		switch (f[i].f_len) {
		case 8:		t = i8ty;	break;
		case 16:	t = i16ty;	break;
		case 32:	t = i32ty;	break;
		case 64:	t = i64ty;	break;
		case 128:	t = i128ty;	break;
		case 256:	t = i256ty;	break;
		default:
			std::cerr << "UGH w=" << f[i].f_len << '\n';
			assert( 0 == 1 && "BAD FIELD WIDTH");
		}

		for (unsigned int c = 0; c < f[i].f_count; c++) {
			types.push_back(t);
		}
	}

	return StructType::create(gctx, types, "guestCtxTy");
}
