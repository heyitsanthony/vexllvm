#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Intrinsics.h>

#include <vector>
#include <iostream>

#include "gueststate.h"

#include "genllvm.h"

GenLLVM* theGenLLVM;

using namespace llvm;

GenLLVM::GenLLVM(const char* name)
{
	builder = new IRBuilder<>(getGlobalContext());
	mod = new Module(name, getGlobalContext());
	guestState = new GuestState();
	mod->addTypeName("guestCtxTy", guestState->getTy());

	ctxData = new GlobalVariable(
		*mod,
		guestState->getTy(),
		false,	/* not constant */
		GlobalVariable::InternalLinkage,
		NULL,
		"genllvm_ctxdata");
	ctxData->dump();
	std::cerr << "......." << std::endl;

	mkFuncTy();
}

GenLLVM::~GenLLVM(void)
{
	delete guestState;
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
//	case Ity_I128:	return "I128";
//	case Ity_V128:	return "V128";
	default:
		std::cout << "COULDN'T HANDLE " << ty << std::endl;
	}
	return NULL;
}

Value* GenLLVM::readCtx(unsigned int byteOff)
{
	unsigned int 	idx;
	Value		*ret;
	
	idx = guestState->byteOffset2ElemIdx(byteOff);
	ret = builder->CreateStructGEP(ctxData, idx, "readCtx");
	ret = builder->CreateLoad(ret);
	
	return ret;
}

Value* GenLLVM::writeCtx(unsigned int byteOff, Value* v)
{
	unsigned int 	idx;
	Value		*ret;
	
	idx = guestState->byteOffset2ElemIdx(byteOff);
	ret = builder->CreateStructGEP(ctxData, idx, "readCtx");
	ret = builder->CreateStore(v, ret);
	
	return ret;

}

void GenLLVM::store(llvm::Value* addr_v, llvm::Value* data_v)
{
	Type	*ptrTy;
	Value	*addr_ptr;

	ptrTy = llvm::PointerType::get(data_v->getType(), 0);
	addr_ptr = builder->CreateBitCast(addr_v, ptrTy, "storePtr");
	builder->CreateStore(data_v, addr_ptr);
}

Value* GenLLVM::load(llvm::Value* addr_v, IRType vex_type)
{
	Type	*ptrTy;
	Value	*addr_ptr;

	ptrTy = llvm::PointerType::get(vexTy2LLVM(vex_type), 0);
	addr_ptr = builder->CreateBitCast(addr_v, ptrTy, "loadPtr");
	return builder->CreateLoad(addr_ptr);
}

/* llvm-ized VexSB functions take form of 
 * guestaddr_t f(gueststate*) {  ...bullshit...; return ctrl_xfer_addr; } */
void GenLLVM::mkFuncTy(void)
{
	std::vector<const llvm::Type*>	f_args;
	f_args.push_back(llvm::PointerType::get(guestState->getTy(), 0));
	funcTy = FunctionType::get(
		builder->getInt64Ty(),
		f_args,
		false);
}
