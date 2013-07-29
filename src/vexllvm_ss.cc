#include "Sugar.h"

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>


#include "elfimg.h"
#include "guestcpustate.h"
#include "guestptimg.h"
#include "procargs.h"

void dumpIRSBs(void)
{ std::cerr << "DUMPING LOGS" << std::cerr; }

GuestPTImg* createAttached(void)
{
	ProcArgs	*pa;
	int		pid;

	pid = atoi(getenv("VEXLLVM_ATTACH"));
	pa = new ProcArgs(pid);
	return GuestPTImg::createAttached<GuestPTImg>(
		pid, pa->getArgc(), pa->getArgv(), pa->getEnv());
}

int main(int argc, char* argv[], char* envp[])
{
	GuestPTImg	*gs;
	const char	*saveas;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s program_path <args>\n", argv[0]);
		return -1;
	}

	if (getenv("VEXLLVM_ATTACH") != NULL) {
		gs = createAttached();
	} else {
		gs = GuestPTImg::create<GuestPTImg>(argc - 1, argv + 1, envp);
	}

	if (saveas = getenv("VEXLLVM_SAVEAS")) {
		fprintf(stderr, "Saving as %s...\n", saveas);
		gs->save(saveas);
	} else {
		fprintf(stderr, "Saving...\n");
		gs->save();
	}

	delete gs;

	return 0;
}
