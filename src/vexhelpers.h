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
	static VexHelpers* create(Arch::Arch);
	virtual ~VexHelpers();
	mod_list getModules(void) const;
	virtual llvm::Function* getHelper(const char* s) const;
	void bindToExeEngine(llvm::ExecutionEngine*);
	void loadUserMod(const char* path);
protected:
	VexHelpers(Arch::Arch arch);
	virtual void loadDefaultModules(void);
	virtual llvm::Module*	loadMod(const char* path);
private:
	Arch::Arch		arch;
	llvm::Module*		helper_mod;
	llvm::Module*		vexop_mod;
	mod_list		user_mods;
	const char		*bc_dirpath;
};

extern VexHelpers* theVexHelpers;

#endif