#include "Sugar.h"

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "vexexecchk.h"
#include "vexexecfastchk.h"
#include "vexcpustate.h"
#include "ptimgchk.h"

static VexExec *vexexec;

void dumpIRSBs(void)
{
	std::cerr << "DUMPING LOG\n";
	vexexec->dumpLogs(std::cerr);
}

int main(int argc, char* argv[], char* envp[])
{
	PTImgChk	*gs;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s program_path <args>\n", argv[0]);
		return -1;
	}

	VexCPUState::registerCPUs();

	gs = GuestPTImg::create<PTImgChk>(argc - 1, argv + 1, envp);
	if (getenv("VEXLLVM_XCHK_SAVE")) {
		fprintf(stderr, "XCHK SAVING\n");
		gs->save();
	}

	if (getenv("VEXLLVM_FASTCHK"))
		vexexec = VexExec::create<VexExecFastChk,PTImgChk>(gs);
	else
		vexexec = VexExec::create<VexExecChk,PTImgChk>(gs);

	assert (vexexec && "Could not create vexexec");
	
	vexexec->run();

	delete vexexec;
	delete gs;

	return 0;
}
