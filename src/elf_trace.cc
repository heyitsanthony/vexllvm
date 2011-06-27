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
#include "guestelf.h"

using namespace llvm;

static VexExec *vexexec;

void dumpIRSBs(void)
{
	std::cerr << "DUMPING LOGS" << std::cerr;
	vexexec->dumpLogs(std::cerr);
}

static void dump_data(void)
{	
	const Guest*	gs;
	vexexec_addrs		addr_stack;

	gs = vexexec->getGuest();

	vexexec->dumpLogs(std::cerr);

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
			<< gs->getName(addr_stack.top())
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
	GuestELF	*gs;
	ElfImg		*img;

	/* for the JIT */
	InitializeNativeTarget();

	if (argc < 2) {
		fprintf(stderr, "Usage: %s elf_path\n", argv[0]);
		return -1;
	}

	signal(SIGSEGV, sigsegv_handler);
	signal(SIGBUS, sigbus_handler);

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

	vexexec = VexExec::create<VexExec,Guest>(gs);
	assert (vexexec && "Could not create vexexec");
	
	vexexec->run();

	printf("\nTRACE COMPLETE. SBs=%d\n", vexexec->getSBExecutedCount());
	dump_data();

	delete vexexec;
	delete gs;
	delete img;

	return 0;
}