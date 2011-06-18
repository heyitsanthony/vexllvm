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
{
	builder = new IRBuilder<>(getGlobalContext());
	mod = new Module(name, getGlobalContext());

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

const Type* GenLLVM::vexTy2LLVM(IRType ty) const
{
	switch(ty) {
	case Ity_I1:	return builder->getInt1Ty();
	case Ity_I8:	return builder->getInt8Ty();
	case Ity_I16:	return builder->getInt16Ty();
	case Ity_I32:	return builder->getInt32Ty();
	case Ity_I64:	return builder->getInt64Ty();
	case Ity_F32:	return builder->getFloatTy();
	case Ity_F64:	return builder->getDoubleTy();
//	case Ity_I128:	 TODO
	case Ity_V128:
		return VectorType::get(
			Type::getInt8Ty(getGlobalContext()), 16);
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
	addr_ptr = builder->CreateIntToPtr(addr_v, ptrTy, "loadPtr");
	loadInst = builder->CreateLoad(addr_ptr);
	loadInst->setAlignment(8);
	return loadInst;
}

Value* GenLLVM::load(Value* addr_v, IRType vex_type)
{
	return load(addr_v, vexTy2LLVM(vex_type));
}

/* gets i8* of base ptr */
Value* GenLLVM::getCtxBase(void)
{
	Value		*intptr_v;
	const Type	*ptrty;

	ptrty = PointerType::get(builder->getInt8Ty(), 0);
	intptr_v = ConstantInt::get(
		getGlobalContext(),
		APInt(	sizeof(intptr_t)*8,
			(uintptr_t)guest->getCPUState()->getStateData()));

	return builder->CreateIntToPtr(intptr_v, ptrty, "ctxbaseptr");
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
	f_args.push_back(PointerType::get(
		guest->getCPUState()->getTy(), 0));
	if(log_last_store) {
		f_args.push_back(PointerType::get(
			MemLog::getType(), 0));
	}
	funcTy = FunctionType::get(
		builder->getInt64Ty(),
		f_args,
		false);
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
