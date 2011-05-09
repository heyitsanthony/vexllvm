#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include "llvm/Target/TargetSelect.h"
#include "llvm/ExecutionEngine/JIT.h"

#include "Sugar.h"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>


#include "elfimg.h"
#include "vexexec.h"
#include "gueststateelf.h"

using namespace llvm;

static VexExec *vexexec;

void dumpIRSBs(void)
{
	std::cerr << "DUMPING LOGS" << std::cerr;
	vexexec->dumpLogs(std::cerr);
}

void sigsegv_handler(int v)
{
	const GuestState*	gs;
	vexexec_addrs		addr_stack;

	gs = vexexec->getGuestState();

	vexexec->dumpLogs(std::cerr);

	std::cerr << "SIGSEGV. TRACE:" << std::endl;

	foreach(it, vexexec->getTraces().begin(), vexexec->getTraces().end()) {
		for (int i = (*it).second; i; i--) 
			std::cerr << " ";
		std::cerr << (*it).first <<std::endl;
	}

	std::cerr << "ADDR STACK:" << std::endl;
	addr_stack = vexexec->getAddrStack();
	while (!addr_stack.empty()) {
		std::cerr
			<< "ADDR:"
			<< gs->getName((guestptr_t)addr_stack.top())
			<< std::endl;
		addr_stack.pop();
	}

	gs->print(std::cout);

	exit(-1);
}

int main(int argc, char* argv[])
{
	GuestState	*gs;
	ElfImg		*img;

	/* for the JIT */
	InitializeNativeTarget();

	if (argc != 2) {
		fprintf(stderr, "Usage: %s elf_path\n", argv[0]);
		return -1;
	}

	signal(SIGSEGV, sigsegv_handler);

	img = ElfImg::create(argv[1]);
	if (img == NULL) {
		fprintf(stderr, "%s: Could not open ELF %s\n", 
			argv[0], argv[1]);
		return -2;
	}

	gs = new GuestStateELF(img);
	vexexec = VexExec::create(gs);
	assert (vexexec && "Could not create vexexec");
	
	vexexec->run();

	printf("\nTRACE COMPLETE. SBs=%d\n", vexexec->getSBExecutedCount());

	delete vexexec;

	return 0;
}