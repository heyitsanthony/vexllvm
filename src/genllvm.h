#ifndef GENLLVM_H
#define GENLLVM_H

/* fuck it */
#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Support/IRBuilder.h>

extern "C" {
#include <valgrind/libvex.h>
#include <valgrind/libvex_ir.h>
}

#include <string>
#include <map>
#include <stdint.h>

class Guest;

struct guest_ctx_field;

class GenLLVM
{
public:
	GenLLVM(const Guest* gs, const char* modname = "vexllvm");
	virtual ~GenLLVM(void);

	llvm::IRBuilder<>* getBuilder(void) { return builder; }
	llvm::Module* getModule(void) { return mod; }

	const llvm::Type* vexTy2LLVM(IRType ty) const;
	
	llvm::Value* readCtx(unsigned int byteOff, IRType ty);
	llvm::Value* readCtx(
		unsigned int byteOff, int bias, int len, llvm::Value* ix, const llvm::Type* t);
	llvm::Value* writeCtx(unsigned int byteOff, llvm::Value* v);
	llvm::Value* writeCtx(
		unsigned int byteOff, int bias, int len,
		llvm::Value* ix, llvm::Value* v);
	llvm::Value* getCtxGEP(unsigned int byteOff, const llvm::Type* ty);
	llvm::Value* getCtxGEP(llvm::Value* byteOff, const llvm::Type* ty);
	llvm::Value* getCtxBase(void);

	void markLinked();
	llvm::Value* getLinked();
	
	void store(llvm::Value* addr, llvm::Value* data);
	llvm::Value* load(llvm::Value* addr_v, IRType vex_type);
	llvm::Value* load(llvm::Value* addr_v, const llvm::Type* ty);
	void beginBB(const char* name);
	llvm::Function* endBB(llvm::Value*);
	void setExitType(uint8_t exit_type);
	llvm::Value* to16x8i(llvm::Value*) const;
	void memFence(void);
private:
	void mkFuncTy(void);

	const Guest		*guest;
	llvm::IRBuilder<>*	builder;
	llvm::Module*		mod;
	const llvm::FunctionType*	funcTy;

	/* current state data */
	llvm::Value*		cur_guest_ctx;
	llvm::Value*		cur_memory_log;

	llvm::Function*		cur_f;
	llvm::BasicBlock*	cur_bb;
	bool			log_last_store;

};

extern class GenLLVM* theGenLLVM;

#endif
