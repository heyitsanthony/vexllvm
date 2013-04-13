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
	void useExternalMod(llvm::Module* m);

	static llvm::Module* loadModFromPath(const char* path);
protected:
	VexHelpers(Arch::Arch arch);
	virtual void loadDefaultModules(void);
	virtual llvm::Module*	loadMod(const char* path);
private:
	void destroyMods(void);

	Arch::Arch		arch;
	llvm::Module*		helper_mod;
	llvm::Module*		vexop_mod;
	mod_list		user_mods;
	const char		*bc_dirpath;

	llvm::Module*		ext_mod;
};

class VexHelperDummy : public VexHelpers
{
public:
	VexHelperDummy(Arch::Arch a) : VexHelpers(a)
	{}
	virtual ~VexHelperDummy() {}
	virtual llvm::Function* getHelper(const char* s) const
	{
		/* can't be null or we'll wind up
		 * terminating early when we hit a ccall */
		return (llvm::Function*)0xb055cafe;
	}

protected:
	virtual llvm::Module* loadMod(const char* path);
};


extern VexHelpers* theVexHelpers;

#endif