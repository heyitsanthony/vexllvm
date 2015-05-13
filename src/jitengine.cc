#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Transforms/Scalar.h>

#include <iostream>
#include <sstream>

#include "jitengine.h"

using namespace llvm;
using namespace llvm::legacy;

class SymMemMgr : public SectionMemoryManager
{
	SymMemMgr(const SymMemMgr&) = delete;
	void operator=(const SymMemMgr&) = delete;
public:
	SymMemMgr(JITEngine &je) : jit_engine(je) {}
	virtual ~SymMemMgr() {}

	/// This method returns the address of the specified function. 
	/// Our implementation will attempt to find functions in other
	/// modules associated with the MCJITHelper to cross link functions
	/// from one generated module to another.
	void* getPointerToNamedFunction(const std::string &name, bool abort_on_fail)
		override;

	uint64_t getSymbolAddress(const std::string& n) override {
		uint64_t addr;
		addr = SectionMemoryManager::getSymbolAddress(n);
		if (addr) return addr;
		return (uint64_t)jit_engine.getPointerToNamedFunction(n);
	}

private:
	JITEngine &jit_engine;
};

void* SymMemMgr::getPointerToNamedFunction(const std::string &name, bool abort_on_fail)
{
	void *pfn;
	
	pfn = SectionMemoryManager::getPointerToNamedFunction(name, false);
	if (pfn) return pfn;

	pfn = jit_engine.getPointerToNamedFunction(name);
	if (!pfn && abort_on_fail) {
		report_fatal_error(
			"Program used external function '" + name +
			"' which could not be resolved!");
		abort();
	}

	return pfn;
}

bool JITEngine::targets_inited = false;

JITEngine::JITEngine(void)
{
	if (!targets_inited) {
		llvm::InitializeNativeTarget();
		llvm::InitializeAllTargetMCs();
		llvm::InitializeNativeTargetAsmPrinter();
		llvm::InitializeNativeTargetAsmParser();
		targets_inited = true;
	}
}

JITEngine::~JITEngine(void) {}

Function* JITEngine::getFunction(const std::string& func_name)
{
	assert(false && "STUB");
}


void* JITEngine::getPointerToNamedFunction(const std::string &name)
{
	for (auto &e : exe_engines) {
		void *fptr;
		if ((fptr = (void*)(e->getFunctionAddress(name))))
			return fptr;
	}
	return nullptr;
}

void* JITEngine::getPointerToFunction(	Function* f,
					std::unique_ptr<llvm::Module> m)
{
	moveModule(std::move(m));
	return getPointerToFunction(f);
}

void* JITEngine::getPointerToFunction(Function* f)
{
	// See if an existing instance of MCJIT has this function.
	// This is probably super slow but there's caching to speed it up.
	void *fptr;
	for (auto &e : exe_engines) {
		if ((fptr = e->getPointerToFunction(f)))
			return fptr;
	}

	// If we didn't find the function, see if we can generate it.
	if (!open_mod) return nullptr;

	std::unique_ptr<ExecutionEngine> new_engine;
	
	new_engine = mod_to_engine(std::move(open_mod));
	fptr = new_engine->getPointerToFunction(f);
	exe_engines.push_back(std::move(new_engine));

	return fptr;
}

std::unique_ptr<ExecutionEngine> JITEngine::mod_to_engine(
	std::unique_ptr<llvm::Module> m)
{
	runPasses(*m);
	std::string	err_str;
	std::unique_ptr<ExecutionEngine> new_engine(
		EngineBuilder(std::move(m))
			.setErrorStr(&err_str)
			.setEngineKind(EngineKind::JIT)
			.setMCJITMemoryManager(std::make_unique<SymMemMgr>(*this))
			.create());
	// XXX selectTarget?

	if (!new_engine) {
		std::cerr << "JITEngine: Could not create ExecutionEngine: "
			  << err_str << '\n';
		abort();
	}

	new_engine->finalizeObject();
	return new_engine;
}

Module& JITEngine::getModuleForNewFunction(void)
{
	if (open_mod) return *open_mod;

	std::stringstream	ss;
	std::string		s;
	ss << "mcjit_module_" << exe_engines.size();
	s = ss.str();
	open_mod = std::make_unique<Module>(	s.c_str(),
						getGlobalContext());
	open_mod->setDataLayout(
		"e-m:e-i64:64-f80:128-n8:16:32:64-S128");
//		"e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:"
//		"64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:"
//		"0:64-s0:64:64-f80:128:128-n8:16:32:64-S128");
	open_mod->setTargetTriple("x86_64-pc-linux-gnu");
	return *open_mod;
}

void JITEngine::runPasses(llvm::Module& m)
{
	// Create a function pass manager for this engine
	auto fpm = std::make_unique<FunctionPassManager>(&m);

     // Set up the optimizer pipeline.  Start with registering info about how the
    // target lays out data structures.
//    fpm->add(new DataLayoutPass(*NewEngine->getDataLayout()));

	// Provide basic AliasAnalysis support for GVN.
	fpm->add(createBasicAliasAnalysisPass());
	// Promote allocas to registers.
	fpm->add(createPromoteMemoryToRegisterPass());
	// Do simple "peephole" optimizations and bit-twiddling optzns.
	fpm->add(createInstructionCombiningPass());
	// Reassociate expressions.
	fpm->add(createReassociatePass());
	// Eliminate Common SubExpressions.
	fpm->add(createGVNPass());
	// Simplify the control flow graph (deleting unreachable blocks, etc).
	fpm->add(createCFGSimplificationPass());
	fpm->doInitialization();
	// For each function in the module

	for (auto &f : m) fpm->run(f);
}

void JITEngine::moveModule(std::unique_ptr<llvm::Module> mod)
{
	assert (mod);
	exe_engines.push_back(mod_to_engine(std::move(mod)));
	assert (!mod);
}
