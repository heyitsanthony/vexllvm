#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Intrinsics.h>

#include <iostream>

#include "gueststate.h"

#include "genllvm.h"

GenLLVM* theGenLLVM;

using namespace llvm;

GenLLVM::GenLLVM(const char* name)
{
	builder = new IRBuilder<>(llvm::getGlobalContext());
	mod = new Module(name, llvm::getGlobalContext());
	guestState = new GuestState();
}

GenLLVM::~GenLLVM(void)
{
	delete guestState;
	delete mod;
	delete builder;
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

	std::cerr << byteOff << " byteoff -> " << idx << " elem idx\n";
	ret = builder->CreateExtractValue(
		builder->CreateLoad(guestState->getLValue()),
		idx);
	
	return ret;
}

Value* GenLLVM::writeCtx(unsigned int byteOff, Value* v)
{
	assert (0 == 1 && "STUB");
}
