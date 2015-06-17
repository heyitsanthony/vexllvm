#ifndef JIT_ENGINE_H
#define JIT_ENGINE_H

#include <vector>
#include <unordered_map>

namespace llvm
{
	class Function;
	class Module;
	class ExecutionEngine;
}

class JITMem;
class JITObjectCache;

class JITEngine
{
public:
	JITEngine(void);
	~JITEngine(void);
	llvm::Function* getFunction(const std::string& func_name);
	llvm::Module&	getModuleForNewFunction(void);
	void*	getPointerToFunction(	llvm::Function*,
					std::unique_ptr<llvm::Module>);
	void*	getPointerToFunction(llvm::Function* f);
	void* 	getPointerToNamedFunction(const std::string &name);
	void	moveModule(std::unique_ptr<llvm::Module>);
private:
	std::unique_ptr<llvm::ExecutionEngine> mod_to_engine(
		std::unique_ptr<llvm::Module>);

	void runPasses(llvm::Module& m);

	std::unique_ptr<llvm::Module>	open_mod;	// unjited module

	static bool targets_inited;

	std::map<std::string, void*>		func_addrs;
	std::unique_ptr<JITMem>			jit_mem;
	std::unique_ptr<JITObjectCache>		jit_objcache;
	unsigned	jit_module_c;
};

#endif
