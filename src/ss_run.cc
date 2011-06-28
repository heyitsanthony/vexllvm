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


#include "vexexec.h"
#include "guestcpustate.h"
#include "guestsnapshot.h"

using namespace llvm;

static VexExec *vexexec;

void dumpIRSBs(void)
{
	std::cerr << "DUMPING LOGS" << std::cerr;
	vexexec->dumpLogs(std::cerr);
}

int main(int argc, char* argv[], char* envp[])
{
	Guest	*g;

	/* for the JIT */
	InitializeNativeTarget();

	GuestMem* mem = new GuestMem();
	if (argc == 1) {
		g = Guest::load(mem);
		if (!g) {
			fprintf(stderr,
				"%s: Couldn't load guest. "
				"Did you run VEXLLVM_SAVE?\n",
			argv[0]);
		}
	} else {
		g = Guest::load(mem, argv[1]);
	}

	assert (g && "Could not load guest");
	vexexec = VexExec::create<VexExec,Guest>(g);
	assert (vexexec && "Could not create vexexec");

	vexexec->run();

	delete vexexec;
	delete g;

	return 0;
}
