#include "Sugar.h"

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "vexexec.h"
#include "vexcpustate.h"
#include "guestsnapshot.h"

using namespace llvm;

static VexExec *vexexec;

void dumpIRSBs(void)
{
	std::cerr << "DUMPING LOGS\n";
	vexexec->dumpLogs(std::cerr);
}

int main(int argc, char* argv[], char* envp[])
{
	std::unique_ptr<Guest>	g;

	VexCPUState::registerCPUs();

	if (argc == 1) {
		g = Guest::load();
		if (!g) {
			fprintf(stderr,
				"%s: Couldn't load guest. "
				"Did you run VEXLLVM_SAVE?\n",
			argv[0]);
		}
	} else {
		g = Guest::load(argv[1]);
	}

	assert (g && "Could not load guest");
	vexexec = VexExec::create<VexExec,Guest>(g.get());
	assert (vexexec && "Could not create vexexec");

	vexexec->run();

	delete vexexec;
	return 0;
}
