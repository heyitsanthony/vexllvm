/* handlers loading the libVex helper functions for ccalls.
 * Also provides support for some of the more tricky ops (e.g. ctz) */
#ifndef VEXHELPERS_H
#define VEXHELPERS_H

#include <list>

namespace llvm
{
class Module;
class Function;
class ExecutionEngine;
}

class VexHelpers
{
public:
	VexHelpers();
	~VexHelpers();
	std::list<llvm::Module*> getModules(void) const;
	llvm::Function* getHelper(const char* s) const;
	void bindToExeEngine(llvm::ExecutionEngine*) const;
private:
	llvm::Module*	loadMod(const char* path);
	llvm::Module	*helper_mod;
	llvm::Module	*vexop_mod;
};

extern VexHelpers* theVexHelpers;

#endif