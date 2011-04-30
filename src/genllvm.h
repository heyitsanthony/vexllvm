#ifndef GENLLVM_H
#define GENLLVM_H

/* fuck it */
#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Support/raw_os_ostream.h>


extern "C" {
#include <valgrind/libvex.h>
#include <valgrind/libvex_ir.h>
}

#include <string>
#include <map>
#include <stdint.h>

class GuestState;

struct guest_ctx_field;

class GenLLVM
{
public:
	GenLLVM(const char* modname = "vexllvm");
	virtual ~GenLLVM(void);

	llvm::IRBuilder<>* getBuilder(void) { return builder; }
	llvm::Module* getModule(void) { return mod; }

	const llvm::Type* vexTy2LLVM(IRType ty) const;
	
	llvm::Value* readCtx(unsigned int byteOff);
	llvm::Value* writeCtx(unsigned int byteOff, llvm::Value* v);
private:
	GuestState		*guestState;
	llvm::IRBuilder<>*	builder;
	llvm::Module*		mod;
};

extern class GenLLVM* theGenLLVM;

#endif
