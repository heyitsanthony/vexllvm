#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Intrinsics.h>

#include <vector>
#include <iostream>

#include "guest.h"
#include "guestcpustate.h"

#include "genllvm.h"
#include "memlog.h"

GenLLVM* theGenLLVM;

using namespace llvm;

GenLLVM::GenLLVM(const Guest* gs, const char* name)
: guest(gs)
, funcTy(NULL)
, cur_guest_ctx(NULL)
, cur_memory_log(NULL)
, cur_f(NULL)
, cur_bb(NULL)
, log_last_store(getenv("VEXLLVM_LAST_STORE"))
, fake_vsys_reads(getenv("VEXLLVM_FAKE_VSYS") != NULL)
{
	builder = new IRBuilder<>(getGlobalContext());
	assert (builder != NULL && "Could not create builder");

	mod = new Module(name, getGlobalContext());
	assert (mod != NULL && "Could not create mod");

	// *any* data layout *should* work, but klee will horribly fail
	// if not LE and the vexllvm loads/stores would fail since it
	// ignores the ordering suffixes. Everything is broken; support
	// BE if/when it finally matters. Right now, force LE.
	//
	// Layout string taken from the vexops.bc file. Good as any other.
	mod->setDataLayout(
		"e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:"
		"32-i64:64:64-f32:3232-f64:64:64-v64:64:64-v128:128:"
		"128-a0:0:64-s0:64:64-f80:128:128");

	assert (guest->getCPUState() && "No CPU state set in Guest");
	mod->addTypeName("guestCtxTy", guest->getCPUState()->getTy());
	mkFuncTy();
}

GenLLVM::~GenLLVM(void)
{
	delete mod;
	delete builder;
}

void GenLLVM::beginBB(const char* name)
{
	assert (cur_bb == NULL && "nested beginBB");
	cur_f = Function::Create(
		funcTy,
		Function::ExternalLinkage,
		name,
		mod);

	cur_bb = BasicBlock::Create(getGlobalContext(), "entry", cur_f);
	builder->SetInsertPoint(cur_bb);
	Function::arg_iterator arg = cur_f->arg_begin();
	cur_guest_ctx = arg++;
	if (log_last_store) {
		cur_memory_log = arg++;
	}
}

Function* GenLLVM::endBB(Value* retVal)
{
	Function	*ret_f;

	assert (cur_bb != NULL && "ending missing bb");

	/* FIXME. Should return next addr to jump to */
	builder->CreateRet(retVal);

	ret_f = cur_f;
	cur_f = NULL;
	cur_bb = NULL;
	cur_guest_ctx = NULL;
	cur_memory_log = NULL;
	return ret_f;
}

const Type* GenLLVM::vexTy2LLVM(IRType ty)
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
	default:
		std::cout << "COULDN'T HANDLE " << ty << std::endl;
	}
	return NULL;
}

Value* GenLLVM::readCtx(unsigned int byteOff, IRType ty)
{
	Value	*ret;

	ret = getCtxGEP(byteOff, vexTy2LLVM(ty));
	ret = builder->CreateLoad(ret);

	return ret;
}

Value* GenLLVM::readCtx(unsigned int byteOff, int bias, int len,
	Value* ix, const Type* accessTy)
{
	Value		*ret, *addr;

	assert(byteOff % (accessTy->getPrimitiveSizeInBits() / 8) == 0);
	const Type* offset_type = IntegerType::get(
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
	addr = getCtxGEP(
		offset,
		accessTy); // XXX check for vector values?
	ret = builder->CreateLoad(addr);

	return ret;
}

Value* GenLLVM::getCtxGEP(unsigned int byteOff, const Type* accessTy)
{
	unsigned int	tyBits;
	tyBits = accessTy->getPrimitiveSizeInBits();
	assert (tyBits && "Access type is 0 bits???");
	return getCtxGEP(
		ConstantInt::get(
			getGlobalContext(),
			APInt(
				32,
				(byteOff*8)/tyBits)),
		accessTy);
}

Value* GenLLVM::getCtxGEP(Value* off, const Type* accessTy)
{
	const Type	*ptrTy;
	Value		*addr_ptr, *ret; /* XXX assuming access are aligned */
	const char	*gep_name;

	ptrTy = PointerType::get(accessTy, 0);

	addr_ptr = builder->CreateBitCast(cur_guest_ctx, ptrTy, "regCtxPtr");

	gep_name = NULL;
	if (isa<ConstantInt>(off)) {
		ConstantInt*	c_off = dyn_cast<ConstantInt>(off);
		uint64_t	off_u64;

		off_u64 = c_off->getZExtValue();
		gep_name = guest->getCPUState()->off2Name(
			(accessTy->getPrimitiveSizeInBits()/8) * off_u64);
	}

	if (gep_name == NULL) gep_name = "unkCtxPtr";

	ret = builder->CreateGEP(addr_ptr, off, gep_name);
	return ret;
}

/* TODO: log these */
Value* GenLLVM::writeCtx(unsigned int byteOff, Value* v)
{
	Value		*ret, *addr;
	StoreInst	*si;

	addr = getCtxGEP(byteOff, v->getType());
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
	const Type* offset_type = IntegerType::get(
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
	addr = getCtxGEP(offset, v->getType());
	si = builder->CreateStore(v, addr);
	ret = si;
	return ret;
}


void GenLLVM::store(Value* addr_v, Value* data_v)
{
	Type		*ptrTy;
	Value		*addr_ptr;
	StoreInst	*si;

	if(cur_memory_log) {
		MemLog::recordStore(cur_memory_log, addr_v, data_v);
	}

	ptrTy = PointerType::get(data_v->getType(), 0);
#ifdef __amd64__
	if(guest->getMem()->is32Bit()) {
		addr_v = builder->CreateZExt(addr_v, IntegerType::get(
			getGlobalContext(), sizeof(void*)*8));
	}
#endif
	if (guest->getMem()->getBase()) {
		addr_v = builder->CreateAdd(addr_v,
			ConstantInt::get(getGlobalContext(),
				APInt(sizeof(intptr_t)*8,
				(uintptr_t)guest->getMem()->getBase())));
	}
	addr_ptr = builder->CreateIntToPtr(addr_v, ptrTy, "storePtr");
	si = builder->CreateStore(data_v, addr_ptr);
	si->setAlignment(8);
	
}

Value* GenLLVM::load(Value* addr_v, const Type* ty)
{
	Type		*ptrTy;
	Value		*addr_ptr;
	LoadInst	*loadInst;

	ptrTy = PointerType::get(ty, 0);
#ifdef __amd64__
	if(guest->getMem()->is32Bit()) {
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
		sys_addr = guest->getMem()->getSysHostAddr(
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


	if (guest->getMem()->getBase()) {
		addr_v = builder->CreateAdd(addr_v,
			ConstantInt::get(getGlobalContext(),
				APInt(sizeof(intptr_t)*8,
				(uintptr_t)guest->getMem()->getBase())));
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

llvm::Value* GenLLVM::getLinked() {
	return ConstantInt::get(getGlobalContext(), APInt(1, 1));
}

void GenLLVM::setExitType(uint8_t exit_type)
{
	writeCtx(
		guest->getCPUState()->getExitTypeOffset(),
		ConstantInt::get(
			getGlobalContext(),
			APInt(8, exit_type)));
}

/* llvm-ized VexSB functions take form of
 * guestaddr_t f(guest*) {  ...bullshit...; return ctrl_xfer_addr; } */
void GenLLVM::mkFuncTy(void)
{
	std::vector<const Type*>	f_args;

	f_args.push_back(PointerType::get(guest->getCPUState()->getTy(), 0));
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

void GenLLVM::memFence(void)
{
	Function  *fence_f;

	/* fence everything! */
	Value* args[5];
	for (int i = 0; i < 5; i++)
		args[i] = ConstantInt::get(builder->getInt1Ty(), 1),

	fence_f = Intrinsic::getDeclaration(mod, Intrinsic::memory_barrier);
	builder->CreateCall(fence_f, args, args+5);
}
