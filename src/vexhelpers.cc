#include "llvm/Module.h"
//#include "llvm/ModuleProvider.h" // XXX
#include "llvm/Instructions.h"

#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/TypeBuilder.h"
#include "llvm/Support/system_error.h"
//#include "llvm/System/Signals.h"

#include <iostream>
#include "genllvm.h"

#include "vexhelpers.h"

#define HELPER_BC_PATH "support/libvex_amd64_helpers.bc"

using namespace llvm;

VexHelpers* theVexHelpers;

VexHelpers::VexHelpers()
: helper_mod(0)
{
	OwningPtr<MemoryBuffer> Buffer;
	bool			materialize_fail;
	std::string		ErrorMsg;

	MemoryBuffer::getFile(HELPER_BC_PATH, Buffer);

	assert (Buffer && "Couldn't get mem buffer on "HELPER_BC_PATH);
//	helper_mod = getLazyBitcodeModule(
//		Buffer.get(), getGlobalContext(), &ErrorMsg);
	helper_mod = ParseBitcodeFile(
		Buffer.get(), getGlobalContext(), &ErrorMsg);
	assert (helper_mod && "Couldn't lazy bitcode helper mod");
	materialize_fail = helper_mod->MaterializeAllPermanently(&ErrorMsg);
	if (materialize_fail) {
		std::cerr << "Materialize failed: " << ErrorMsg << std::endl;
		assert (0 == 1 && "BAD MOD");
	}
}

VexHelpers::~VexHelpers()
{
	delete helper_mod;
}

Function* VexHelpers::getHelper(const char* s) const
{
	return helper_mod->getFunction(s);
}
