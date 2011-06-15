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
static int count(char**v) {
	int i = 0;
	for(;v[i]; ++i);
	return i;
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

	img = ElfImg::create(argv[1]);
	if (img == NULL) {
		fprintf(stderr, "%s: Could not open ELF %s\n", 
			argv[0], argv[1]);
		return -2;
	}
	gs = new GuestELF(img);
	gs->setArgv(argc-1, const_cast<const char**>(argv+1),
		count(envp), const_cast<const char**>(envp));
	vexexec = VexExec::create<VexExec, Guest>(gs);
	assert (vexexec && "Could not create vexexec");
	
	vexexec->run();

	delete vexexec;
	delete gs;
	delete img;

	return 0;
}
