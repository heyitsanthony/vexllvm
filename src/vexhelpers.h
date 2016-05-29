/* handlers loading the libVex helper functions for ccalls.
 * Also provides support for some of the more tricky ops (e.g. ctz) */
#ifndef VEXHELPERS_H
#define VEXHELPERS_H

#include "Sugar.h"
#include "arch.h"

namespace llvm
{
class Module;
class Function;
class ExecutionEngine;
}

typedef ptr_list_t<llvm::Module>	umod_list;
typedef std::list<llvm::Module*>	mod_list;
class JITEngine;

class VexHelpers
{
public:
	static std::unique_ptr<VexHelpers> create(Arch::Arch);
	virtual ~VexHelpers();
	umod_list takeModules(void);
	virtual llvm::Function* getHelper(const char* s) const;
	void moveToJITEngine(JITEngine&);
	void loadUserMod(const char* path);
	void useExternalMod(llvm::Module* m);

	static std::unique_ptr<llvm::Module> loadModFromPath(const char* path);
protected:
	VexHelpers(Arch::Arch arch);
	virtual void loadDefaultModules(void);
	virtual std::unique_ptr<llvm::Module>	loadMod(const char* path);
private:
	void destroyMods(void);

	Arch::Arch		arch;
	std::unique_ptr<llvm::Module>		helper_mod;
	std::unique_ptr<llvm::Module>		vexop_mod;
	umod_list		user_mods;
	const char		*bc_dirpath;

	llvm::Module		*ext_mod; // owned by someone else
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
	virtual std::unique_ptr<llvm::Module> loadMod(const char* path);
};


extern std::unique_ptr<VexHelpers> theVexHelpers;

#endif