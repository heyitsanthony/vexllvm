#ifndef DEBUGPRINTPASS_H
#define DEBUGPRINTPASS_H

#include <llvm/Pass.h>

// XXX only works with JIT because of funny string handling
class DebugPrintPass : public llvm::FunctionPass
{
public:
	DebugPrintPass()
		: llvm::FunctionPass(ID)
		, debug_func_ty(getFuncType())
	{
	}

	bool runOnFunction(llvm::Function &f) override;

private:
	llvm::FunctionType* getFuncType(void) const;
	llvm::Type	*debug_func_ty;
	static char	ID;
};

#endif
