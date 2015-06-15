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

#include <set>
#include <iostream>
#include <sstream>

#include "jitengine.h"

using namespace llvm;
using namespace llvm::legacy;

///////////
// Dumb thing to manage memory across multiple execution engines.
// JITMem tracks all JITed functions ever.
// JITMemProxy passes JITed functions to JITMem via exe engine.
////////////

class JITMem : public SectionMemoryManager
{
public:
	JITMem(JITEngine& in_je) : jit_engine(in_je) {}
	virtual ~JITMem() {}

	JITMem(const JITMem&) = delete;
	void operator=(const JITMem&) = delete;

	std::unique_ptr<SectionMemoryManager> createProxy(void);

	void* getPointerToNamedFunction(const std::string &name, bool abort_on_fail)
		override;

	uint64_t getSymbolAddress(const std::string& n) override {
		uint64_t addr;
		addr = SectionMemoryManager::getSymbolAddress(n);
		if (addr) return addr;
		return (uint64_t)jit_engine.getPointerToNamedFunction(n);
	}

	uint8_t *allocateCodeSection(
		uintptr_t Size, unsigned Alignment, unsigned SectionID,
		StringRef SectionName) override
	{
		uint8_t	*ret;

		ret = SectionMemoryManager::allocateCodeSection(
			Size, Alignment, SectionID, SectionName);
		return ret;
	}

	uint8_t *allocateDataSection(
		uintptr_t Size, unsigned Alignment, unsigned SectionID,
		StringRef SectionName, bool isReadOnly) override
	{
		uint8_t *ret;

		ret = SectionMemoryManager::allocateDataSection(
			Size, Alignment, SectionID, SectionName, isReadOnly);
		return ret;
	}

	void reserveAllocationSpace(
		uintptr_t CodeSize, uintptr_t DataSizeRO,
		uintptr_t DataSizeRW) override
	{
		SectionMemoryManager::reserveAllocationSpace(
			CodeSize, DataSizeRO, DataSizeRW);
	}

private:
	JITEngine	&jit_engine;
};

// since SMM is destroyed with each exe engine, have a sacrificial class
// that only passes method calls to the shared JITMem object
class JITMemProxy : public SectionMemoryManager
{
public:
	JITMemProxy(JITMem &jit_mem) : jm(jit_mem) {}

	JITMemProxy(const JITMemProxy&) = delete;
	void operator=(const JITMemProxy&) = delete;

	void* getPointerToNamedFunction(const std::string &name, bool abort_on_fail)
		override
	{ return jm.getPointerToNamedFunction(name, abort_on_fail); }

	uint64_t getSymbolAddress(const std::string& n) override
	{ return jm.getSymbolAddress(n); }

	uint8_t *allocateCodeSection(
		uintptr_t Size, unsigned Alignment, unsigned SectionID,
		StringRef SectionName) override

	{
		return jm.allocateCodeSection(
			Size, Alignment, SectionID, SectionName);
	}

	uint8_t *allocateDataSection(
		uintptr_t Size, unsigned Alignment,
		unsigned SectionID, StringRef SectionName,
		bool isReadOnly) override
	{
		return jm.allocateDataSection(
			Size, Alignment, SectionID, SectionName, isReadOnly);
	}

	void reserveAllocationSpace(
		uintptr_t CodeSize, uintptr_t DataSizeRO,
		uintptr_t DataSizeRW) override
	{
		jm.reserveAllocationSpace(CodeSize, DataSizeRO, DataSizeRW);
	}

private:
	JITMem &jm;
};

// compiler can't figure out JITMemProxy <: SectionMemoryManager
std::unique_ptr<SectionMemoryManager> JITMem::createProxy(void)
{
	return std::make_unique<JITMemProxy>(*this);
}


void* JITMem::getPointerToNamedFunction(const std::string &name, bool abort_on_fail)
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
//////////////

bool JITEngine::targets_inited = false;

JITEngine::JITEngine(void)
	: jit_module_c(0)
{
	if (!targets_inited) {
		if (llvm::InitializeNativeTarget()) abort();
		llvm::InitializeNativeTargetAsmPrinter();
		llvm::InitializeNativeTargetAsmParser();
		targets_inited = true;
	}

	jit_mem = std::make_unique<JITMem>(*this);
}

JITEngine::~JITEngine(void) {}

Function* JITEngine::getFunction(const std::string& func_name)
{
	assert(false && "STUB");
}

void* JITEngine::getPointerToNamedFunction(const std::string &name)
{
	auto it = func_addrs.find(name);
	return (it != func_addrs.end())
		? it->second
		: nullptr;
}

void* JITEngine::getPointerToFunction(	Function* f,
					std::unique_ptr<llvm::Module> m)
{
	void	*fptr;

	std::string	s = f->getName().str();
	if ((fptr = getPointerToNamedFunction(s))) {
		return fptr;
	}
	moveModule(std::move(m));
	fptr = getPointerToNamedFunction(s);
	return fptr;
}

void* JITEngine::getPointerToFunction(Function* f)
{
	void *fptr;
	if ((fptr = getPointerToNamedFunction(f->getName().str())))
		return fptr;

	// If we didn't find the function, see if we can generate it.
	if (!open_mod) return nullptr;

	std::unique_ptr<ExecutionEngine> new_engine;
	new_engine = mod_to_engine(std::move(open_mod));
	return new_engine->getPointerToFunction(f);
}

std::unique_ptr<ExecutionEngine> JITEngine::mod_to_engine(
	std::unique_ptr<llvm::Module> m)
{
	runPasses(*m);

	std::set<std::string> new_fns;
	for (auto & f : *m) {
		std::string fn = f.getName().str();
		assert(!func_addrs.count(fn));
		new_fns.insert(fn);
	}

	std::string	err_str;
	std::unique_ptr<ExecutionEngine> new_engine(
		EngineBuilder(std::move(m))
			.setErrorStr(&err_str)
			.setEngineKind(EngineKind::JIT)
			.setMCJITMemoryManager(jit_mem->createProxy())
			.create());
	// XXX selectTarget?

	if (!new_engine) {
		std::cerr << "JITEngine: Could not create ExecutionEngine: "
			  << err_str << '\n';
		abort();
	}

	new_engine->finalizeObject();
	for (auto & fn : new_fns) {
		void	*fptr = new_engine->getPointerToNamedFunction(fn, false);
		if (!fptr) {
			// std::cerr << "Ignoring " << fn << '\n';
			continue;
		}
		func_addrs[fn] = fptr;
	}

	return new_engine;
}

Module& JITEngine::getModuleForNewFunction(void)
{
	if (open_mod) return *open_mod;

	std::stringstream	ss;
	std::string		s;
	ss << "mcjit_module_" << jit_module_c++;
	s = ss.str();

	open_mod = std::make_unique<Module>(s.c_str(), getGlobalContext());
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
#if 0
	// Create a function pass manager for this engine
	auto fpm = std::make_unique<FunctionPassManager>(&m);

	// Set up the optimizer pipeline.  Start with registering info about how the
	// target lays out data structures.
	// fpm->add(new DataLayoutPass(*NewEngine->getDataLayout()));

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
#endif
}

void JITEngine::moveModule(std::unique_ptr<llvm::Module> mod)
{
	assert (mod);
	mod_to_engine(std::move(mod));
	assert (!mod);
}
