#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>

#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Transforms/Utils/Cloning.h>

#include <iostream>
#include <stdio.h>
#include "Sugar.h"
#include "genllvm.h"

#include "jitengine.h"
#include "vexhelpers.h"

#define VEXOP_BC_FILE "vexops.bc"

using namespace llvm;

std::unique_ptr<VexHelpers> theVexHelpers;
extern void vexop_setup_fp(VexHelpers* vh);

VexHelpers::VexHelpers(Arch::Arch in_arch)
: arch(in_arch)
, ext_mod(nullptr)
{
	/* env not set => assume running from git root */
	bc_dirpath = getenv("VEXLLVM_HELPER_PATH");
	if (bc_dirpath == nullptr) bc_dirpath = "bitcode";
}

void VexHelpers::loadDefaultModules(void)
{
	char		path_buf[512];

	const char* helper_file = nullptr;
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

	assert (vexop_mod && (helper_file == nullptr || helper_mod));
}

std::unique_ptr<VexHelpers> VexHelpers::create(Arch::Arch arch)
{
	VexHelpers	*vh;

	vh = new VexHelpers(arch);
	vh->loadDefaultModules();

	return std::unique_ptr<VexHelpers>(vh);
}

std::unique_ptr<Module> VexHelpers::loadMod(const char* path)
{ return loadModFromPath(path); }

std::unique_ptr<Module> VexHelpers::loadModFromPath(const char* path)
{
	SMDiagnostic	diag;	

	auto ret_mod = llvm::parseIRFile(path, diag, getGlobalContext());
	if (ret_mod == nullptr) {
		std::string	s(diag.getMessage());
		std::cerr
			<< "Error Parsing Bitcode File '"
			<< path << "': " << s << '\n';
	}
	assert (ret_mod && "Couldn't parse bitcode mod");
	auto err = ret_mod->materializeAll();
	if (err) {
		std::cerr << "Materialize failed... " << std::endl;
		assert (0 == 1 && "BAD MOD");
	}

	if (ret_mod == nullptr) {
		std::cerr << "OOPS. No mod. (path=" << path << ")\n";
	}
	return ret_mod;
}

umod_list VexHelpers::takeModules(void)
{
	umod_list	l;
	while (!user_mods.empty()) {
		auto m = (user_mods.front()).release();
		user_mods.pop_front();
		l.emplace_back(m);
	}
	if (helper_mod) l.emplace_back(std::move(helper_mod));
	l.emplace_back(std::move(vexop_mod));
	return l;
}

void VexHelpers::loadUserMod(const char* path)
{
	char	pathbuf[512];

	assert ((strlen(path)+strlen(bc_dirpath)) < 512);
	snprintf(pathbuf, 512, "%s/%s", bc_dirpath, path);

	auto m = loadMod(pathbuf);
	assert (m != nullptr && "Could not load user module");
	user_mods.push_back(std::move(m));
}

void VexHelpers::destroyMods(void)
{
	helper_mod = nullptr;
	vexop_mod = nullptr;
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

	assert (vexop_mod);
	if ((f = vexop_mod->getFunction(s))) return f;

	for (auto &m : user_mods) {
		if ((f = m->getFunction(s)))
			return f;
	}

	return nullptr;
}

void VexHelpers::moveToJITEngine(JITEngine& je)
{
	if (helper_mod)
		je.moveModule(
			std::unique_ptr<Module>(
				CloneModule(helper_mod.get())));
	if (vexop_mod)
		je.moveModule(
			std::unique_ptr<Module>(
				CloneModule(vexop_mod.get())));
}

std::unique_ptr<llvm::Module> VexHelperDummy::loadMod(const char* path)
{
	fprintf(stderr, "FAILING LOAD MOD %s\n", path);
	return nullptr;
}

void VexHelpers::useExternalMod(Module* m)
{
	assert (ext_mod == nullptr);
	destroyMods();
	ext_mod = m;
}
