/* handlers loading the libVex helper functions for ccalls */
#ifndef VEXHELPERS_H
#define VEXHELPERS_H

namespace llvm
{
class Module;
class Function;
}

class VexHelpers
{
public:
	VexHelpers();
	~VexHelpers();
	llvm::Module* getModule(void) { return helper_mod; }
	llvm::Function* getHelper(const char* s) const;
private:
	llvm::Module	*helper_mod;
};

extern VexHelpers* theVexHelpers;

#endif