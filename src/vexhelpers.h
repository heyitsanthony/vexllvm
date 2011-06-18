/* handlers loading the libVex helper functions for ccalls.
 * Also provides support for some of the more tricky ops (e.g. ctz) */
#ifndef VEXHELPERS_H
#define VEXHELPERS_H

#include <list>
#include "arch.h"

namespace llvm
{
class Module;
class Function;
class ExecutionEngine;
}

typedef std::list<llvm::Module*>	mod_list;

class VexHelpers
{
public:
	VexHelpers(Arch::Arch arch);
	~VexHelpers();
	mod_list getModules(void) const;
	llvm::Function* getHelper(const char* s) const;
	void bindToExeEngine(llvm::ExecutionEngine*);
	void loadUserMod(const char* path);
private:
	llvm::Module*	loadMod(const char* path);
	llvm::Module*	helper_mod;
	llvm::Module*	vexop_mod;
	mod_list	user_mods;
	const char	*bc_dirpath;
};

extern VexHelpers* theVexHelpers;

#endif