#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/Module.h>
#include <llvm/Instructions.h>

#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/TypeBuilder.h>
#include <llvm/Support/system_error.h>

#include <iostream>
#include <stdio.h>
#include "Sugar.h"
#include "genllvm.h"

#include "vexhelpers.h"

#define VEXOP_BC_FILE "vexops.bc"

using namespace llvm;

VexHelpers* theVexHelpers;
extern void vexop_setup_fp(VexHelpers* vh);

VexHelpers::VexHelpers(Arch::Arch in_arch)
: arch(in_arch)
, helper_mod(0)
, vexop_mod(0)
, ext_mod(0)
{
	/* env not set => assume running from git root */
	bc_dirpath = getenv("VEXLLVM_HELPER_PATH");
	if (bc_dirpath == NULL) bc_dirpath = "bitcode";
}

void VexHelpers::loadDefaultModules(void)
{
	char		path_buf[512];

	const char* helper_file = NULL;
	switch(arch) {
	case Arch::X86_64: helper_file = "libvex_amd64_helpers.bc"; break;
	case Arch::I386: helper_file = "libvex_x86_helpers.bc"; break;
	case Arch::ARM: helper_file = "libvex_arm_helpers.bc"; break;
	case Arch::MIPS32: break; /* no useful helpers for mips */
	default:
		assert(!"known arch for helpers");
	}

	if (helper_file) {
		snprintf(path_buf, 512, "%s/%s", bc_dirpath, helper_file);
		helper_mod = loadMod(path_buf);
	}

	snprintf(path_buf, 512, "%s/%s", bc_dirpath, VEXOP_BC_FILE);
	vexop_mod = loadMod(path_buf);

	vexop_setup_fp(this);

	assert (vexop_mod && (helper_file == NULL || helper_mod));
}

VexHelpers* VexHelpers::create(Arch::Arch arch)
{
	VexHelpers	*vh;

	vh = new VexHelpers(arch);
	vh->loadDefaultModules();

	return vh;
}

Module* VexHelpers::loadMod(const char* path)
{ return loadModFromPath(path); }

Module* VexHelpers::loadModFromPath(const char* path)
{
	Module			*ret_mod;
	std::string		ErrorMsg;

	OwningPtr<MemoryBuffer> Buffer;
	bool			materialize_fail;

	MemoryBuffer::getFile(path, Buffer);

	if (!Buffer) {
		std::cerr <<  "Bad membuffer on " << path << std::endl;
		assert (Buffer && "Couldn't get mem buffer");
	}

	ret_mod = ParseBitcodeFile(Buffer.get(), getGlobalContext(), &ErrorMsg);
	if (ret_mod == NULL) {
		std::cerr
			<< "Error Parsing Bitcode File '"
			<< path << "': " << ErrorMsg << '\n';
	}
	assert (ret_mod && "Couldn't parse bitcode mod");
	materialize_fail = ret_mod->MaterializeAllPermanently(&ErrorMsg);
	if (materialize_fail) {
		std::cerr << "Materialize failed: " << ErrorMsg << std::endl;
		assert (0 == 1 && "BAD MOD");
	}

	if (ret_mod == NULL) {
		std::cerr << "OOPS: " << ErrorMsg
			<< " (path=" << path << ")\n";
	}
	return ret_mod;
}

mod_list VexHelpers::getModules(void) const
{
	mod_list	l = user_mods;
	if (helper_mod) l.push_back(helper_mod);
	l.push_back(vexop_mod);
	return l;
}

void VexHelpers::loadUserMod(const char* path)
{
	Module*	m;
	char	pathbuf[512];

	assert ((strlen(path)+strlen(bc_dirpath)) < 512);
	snprintf(pathbuf, 512, "%s/%s", bc_dirpath, path);

	m = loadMod(pathbuf);
	assert (m != NULL && "Could not load user module");
	user_mods.push_back(m);
}

void VexHelpers::destroyMods(void)
{
	if (helper_mod) delete helper_mod;
	if (vexop_mod) delete vexop_mod;
	foreach (it, user_mods.begin(), user_mods.end())
		delete (*it);

	helper_mod = NULL;
	vexop_mod = NULL;
	user_mods.clear();
}

VexHelpers::~VexHelpers()
{
	destroyMods();
}

Function* VexHelpers::getHelper(const char* s) const
{
	Function	*f;

	if (ext_mod) {
		return ext_mod->getFunction(s);
	}

	/* TODO why not start using a better algorithm sometime */
	if (helper_mod && (f = helper_mod->getFunction(s))) return f;
	if ((f = vexop_mod->getFunction(s))) return f;
	foreach (it, user_mods.begin(), user_mods.end()) {
		if ((f = (*it)->getFunction(s)))
			return f;
	}

	return NULL;
}

void VexHelpers::bindToExeEngine(ExecutionEngine* exe)
{
	if (helper_mod) exe->addModule(helper_mod);
	exe->addModule(vexop_mod);
}

llvm::Module* VexHelperDummy::loadMod(const char* path)
{
	fprintf(stderr, "FAILING LOAD MOD %s\n", path);
	return NULL;
}

void VexHelpers::useExternalMod(Module* m)
{
	assert (ext_mod == NULL);
	destroyMods();
	ext_mod = m;
}
