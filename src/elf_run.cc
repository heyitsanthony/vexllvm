#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include "llvm/Target/TargetSelect.h"
#include "llvm/ExecutionEngine/JIT.h"

#include "Sugar.h"

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>


#include "elfimg.h"
#include "vexexec.h"
#include "guestcpustate.h"
#include "guestelf.h"

using namespace llvm;

static VexExec *vexexec;

void dumpIRSBs(void)
{
	std::cerr << "DUMPING LOGS" << std::cerr;
	vexexec->dumpLogs(std::cerr);
}
int main(int argc, char* argv[], char* envp[])
{
	GuestELF	*gs;
	ElfImg		*img;

	/* for the JIT */
	InitializeNativeTarget();

	if (argc < 2) {
		fprintf(stderr, "Usage: %s elf_path <cmdline>\n", argv[0]);
		return -1;
	}

	std::vector<char*> env;
	for(int i = 0; envp[i]; ++i)
		env.push_back(envp[i]);
	int skip = 0;
	for(;skip < argc - 1; ++skip) {
		std::string arg = argv[skip + 1];
		if(arg.find('=') == std::string::npos)
			break;
	}
	for(int i = 0; i < skip; ++i) {
		env.push_back(argv[i + 1]);
	}
	GuestMem* mem = new GuestMem();
	img = ElfImg::create(mem, argv[1+skip]);
	if (img == NULL) {
		fprintf(stderr, "%s: Could not open ELF %s\n", 
			argv[0], argv[1+skip]);
		return -2;
	}
	gs = new GuestELF(mem, img);
	gs->setArgv(argc-1-skip, const_cast<const char**>(argv+1+skip),
		env.size(), const_cast<const char**>(&env[0]));

	vexexec = VexExec::create<VexExec, Guest>(gs);
	assert (vexexec && "Could not create vexexec");
	
	vexexec->run();

	delete vexexec;
	delete gs;
	delete img;

	return 0;
}
