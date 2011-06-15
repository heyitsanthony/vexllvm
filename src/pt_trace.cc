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
#include "guestcpustate.h"
#include "guestptimg.h"

using namespace llvm;

static VexExec *vexexec;

void dumpIRSBs(void)
{
	std::cerr << "[VEXLLVM] DUMPING LOGS" << std::cerr;
	vexexec->dumpLogs(std::cerr);
}
static void dump_data(void)
{	
	const Guest*		gs;
	vexexec_addrs		addr_stack;

	gs = vexexec->getGuest();

	vexexec->dumpLogs(std::cerr);

	foreach(it, vexexec->getTraces().begin(), vexexec->getTraces().end()) {
		std::cerr << "[VEXLLVM] ";
		for (int i = (*it).second; i; i--) 
			std::cerr << " ";
		std::cerr << (*it).first <<std::endl;
	}

	std::cerr << "[VEXLLVM] ADDR STACK:" << std::endl;
	addr_stack = vexexec->getAddrStack();
	while (!addr_stack.empty()) {
		std::cerr
			<< "[VEXLLVM] ADDR:"
			<< gs->getName((guestptr_t)addr_stack.top())
			<< std::endl;
		addr_stack.pop();
	}

	gs->print(std::cerr);
}

void sigsegv_handler(int v)
{
	vexexec->dumpLogs(std::cerr);

	std::cerr << "SIGSEGV. TRACE:" << std::endl;
	dump_data();
	signal(SIGSEGV, SIG_DFL);
}

void sigbus_handler(int v)
{
	vexexec->dumpLogs(std::cerr);

	std::cerr << "SIGBUS. TRACE:" << std::endl;
	dump_data();
	signal(SIGBUS, SIG_DFL);
}

int main(int argc, char* argv[], char* envp[])
{
	GuestPTImg	*gs;

	/* for the JIT */
	InitializeNativeTarget();

	if (argc < 2) {
		fprintf(stderr, "Usage: %s program_path <args>\n", argv[0]);
		return -1;
	}

	signal(SIGSEGV, sigsegv_handler);
	signal(SIGBUS, sigbus_handler);

	gs = GuestPTImg::create<GuestPTImg>(argc - 1, argv + 1, envp);
	vexexec = VexExec::create<VexExec,Guest>(gs);
	assert (vexexec && "Could not create vexexec");
	
	vexexec->run();

	printf("\n[VEXLLVM] TRACE COMPLETE. SBs=%d\n", vexexec->getSBExecutedCount());
	dump_data();

	delete vexexec;
	delete gs;

	return 0;
}
