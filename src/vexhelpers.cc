#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/Module.h>
// XXX. Kill when KLEE is updated
#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR == 6)
#include <llvm/ModuleProvider.h>
#endif
#include <llvm/Instructions.h>

#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/TypeBuilder.h>
#include <llvm/Support/system_error.h>
//#include <llvm/System/Signals.h>

#include <iostream>
#include <stdio.h>
#include "genllvm.h"

#include "vexhelpers.h"

#define HELPER_BC_FILE "libvex_amd64_helpers.bc"
#define VEXOP_BC_FILE "vexops.bc"

using namespace llvm;

VexHelpers* theVexHelpers;

VexHelpers::VexHelpers()
: helper_mod(0), vexop_mod(0)
{
	char		path_buf[512];
	const char	*bc_dirpath;
	
	/* env not set => assume running from git root */
	bc_dirpath = getenv("VEXLLVM_HELPER_PATH");
	if (bc_dirpath == NULL) bc_dirpath = "bitcode";

	snprintf(path_buf, 512, "%s/%s", bc_dirpath, HELPER_BC_FILE);
	helper_mod = loadMod(path_buf);
	snprintf(path_buf, 512, "%s/%s", bc_dirpath, VEXOP_BC_FILE);
	vexop_mod = loadMod(path_buf);

	assert (helper_mod && vexop_mod);
}

Module* VexHelpers::loadMod(const char* path)
{
	Module			*ret_mod;
	std::string		ErrorMsg;

/* XXX. Kill this when KLEE is updated to non-ancient. */
#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR == 6)
	ModuleProvider	*MP = 0;
	MemoryBuffer	*Buffer;
	Buffer =  MemoryBuffer::getFileOrSTDIN(std::string(path), &ErrorMsg);
	if (Buffer) {
		MP = getBitcodeModuleProvider(Buffer, getGlobalContext(), &ErrorMsg);
		if (!MP) delete Buffer;
	}

	assert (MP != NULL && "Error loading helper module");
	ret_mod = MP->materializeModule();
//	MP->releaseModule();
//	delete MP;
#else
	OwningPtr<MemoryBuffer> Buffer;
	bool			materialize_fail;

	MemoryBuffer::getFile(path, Buffer);

	if (!Buffer) {
		std::cerr <<  "Bad membuffer on " << path << std::endl;
		assert (Buffer && "Couldn't get mem buffer");
	}

	ret_mod = ParseBitcodeFile(Buffer.get(), getGlobalContext(), &ErrorMsg);
	assert (ret_mod && "Couldn't parse bitcode mod");
	materialize_fail = ret_mod->MaterializeAllPermanently(&ErrorMsg);
	if (materialize_fail) {
		std::cerr << "Materialize failed: " << ErrorMsg << std::endl;
		assert (0 == 1 && "BAD MOD");
	}

#endif

	return ret_mod;
}

std::list<llvm::Module*> VexHelpers::getModules(void) const
{
	std::list<llvm::Module*>	l;
	l.push_back(helper_mod);
	l.push_back(vexop_mod);
	return l;
}

VexHelpers::~VexHelpers()
{
/* XXX. Kill when KLEE is updated */
#if !(LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR == 6)
	delete helper_mod;
	delete vexop_mod;
#endif
}

Function* VexHelpers::getHelper(const char* s) const
{
	Function	*f;
	if ((f = helper_mod->getFunction(s))) return f;
	if ((f = vexop_mod->getFunction(s))) return f;
	return NULL;
}

void VexHelpers::bindToExeEngine(ExecutionEngine* exe)
{
/* XXX. Kill when KLEE is updated */
#if !(LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR == 6)
	exe->addModule(helper_mod);
	exe->addModule(vexop_mod);
#endif
}
