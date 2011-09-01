#include "Sugar.h"

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "vexsb.h"
#include "vexexec.h"
#include "guestcpustate.h"
#include "guestsnapshot.h"
#include "fragcache.h"

using namespace llvm;

static VexExec *vexexec;

void dumpIRSBs(void)
{
	std::cerr << "DUMPING LOGS" << std::cerr;
	vexexec->dumpLogs(std::cerr);
}

int main(int argc, char* argv[])
{
	Guest		*g;
	uint64_t	addr;
	size_t		sz;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s addr [guest-sshot]\n", argv[0]);
		return -1;
	}

	sz = sscanf(argv[1], "0x%lx", &addr);
	if (sz != 1) {
		fprintf(stderr,
			"Failed to read address \"%s\". Want hex.\n", argv[1]);
		return -2;
	}
	printf("[frag-run] Fragment=%p\n", (void*)((intptr_t)addr));

	FragCache* fc = FragCache::create(NULL);
	if (fc) {
		char	*buf;
		int	len;
		buf = GuestSnapshot::readMemory(
			(argc >= 3)
				? argv[2]
				: "guest-last",
			guest_ptr(addr),
			2048);
		assert (buf != NULL);
		len = fc->findLength(buf);
		delete [] buf;
		delete fc;
		if (len != -1) {
			std::cout <<
				"[frag-run] VSB Size=" << len << std::endl;
			return 0;
		}

		/* force storage */
		setenv("VEXLLVM_STORE_FRAGS", "1", 1);
	}

	if (argc == 2) {
		g = Guest::load();
		if (!g) {
			fprintf(stderr,
				"%s: Couldn't load guest. "
				"Did you run VEXLLVM_SAVE?\n",
			argv[0]);
		}
	} else {
		assert (argc >= 3);
		g = Guest::load(argv[2]);
	}

	assert (g && "Could not load guest");
	vexexec = VexExec::create<VexExec,Guest>(g);
	assert (vexexec && "Could not create vexexec");

	g->getCPUState()->setPC(guest_ptr(addr));
//	vexexec->beginStepping();
//	bool ok_step = vexexec->stepVSB();
//	std::cout << "[frag-run] OK-STEP=" << ok_step << std::endl;
	const VexSB* vsb = vexexec->getCachedVSB(guest_ptr(addr));
	if (vsb != NULL) {
		std::cout << "[frag-run] VSB Size=" << vsb->getSize() << std::endl;
	}

	delete vexexec;
	delete g;

	return 0;
}
