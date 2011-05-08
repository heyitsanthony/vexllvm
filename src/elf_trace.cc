#include <llvm/Value.h>
#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Intrinsics.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include "llvm/Target/TargetSelect.h"
#include "llvm/ExecutionEngine/JIT.h"

#include "Sugar.h"
#include <stack>
#include <map>

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "vexxlate.h"
#include "elfimg.h"

extern "C" {
#include <valgrind/libvex.h>
#include <valgrind/libvex_ir.h>
#include <valgrind/libvex_guest_amd64.h>
};

#include "syscalls.h"
#include "vexhelpers.h"
#include "vexsb.h"
#include "gueststate.h"
#include "guestcpustate.h"
#include "genllvm.h"

using namespace llvm;

static ExecutionEngine		*theExeEngine;
static GuestState		*theGuestState;

typedef uint64_t(*vexfunc_t)(void* /* guest state */);

std::stack<elfptr_t>	addr_stack;	/* holds elf addresses */
std::list<std::pair<elfptr_t, int /* depth */> >	trace;

void sigsegv_handler(int v)
{
	std::cerr << "SIGSEGV. TRACE:" << std::endl;
	foreach(it, trace.begin(), trace.end()) {
		for (int i = (*it).second; i; i--) 
			std::cerr << " ";
		std::cerr << (*it).first <<std::endl;
	}

	std::cerr << "ADDR STACK:" << std::endl;
	while (!addr_stack.empty()) {
		std::cerr
			<< "ADDR:"
			<< theGuestState->getName((guestptr_t)addr_stack.top())
			<< std::endl;
		addr_stack.pop();
	}

	theGuestState->print(std::cout);

	exit(-1);
}

static VexSB* vexsb_from_elfaddr(
	VexXlate* vexlate, ElfImg* img, elfptr_t elfptr)
{
	hostptr_t	hostptr;

	hostptr = img->xlateAddr(elfptr);

	/* XXX recongnize library ranges */
	if (hostptr == NULL) hostptr = elfptr;

	return vexlate->xlate(hostptr, (uint64_t)elfptr);
}

uint64_t do_func(Function* f, void* guest_ctx)
{
	/* don't forget to run it! */
	vexfunc_t func_ptr;
	
	func_ptr = (vexfunc_t)theExeEngine->getPointerToFunction(f);
	assert (func_ptr != NULL && "Could not JIT");
	fprintf(stderr, "doing func %p\n", func_ptr);

	return func_ptr(guest_ctx);
}

static VexSB* runNextVexSB(VexXlate* vexlate, ElfImg* img, GuestState* gs)
{
	Syscalls	sc;
	elfptr_t	elfptr;
	llvm::Function	*f;
	elfptr_t	new_jmpaddr;
	VexSB		*vsb;

	elfptr = addr_stack.top();
	addr_stack.pop();
	trace.push_back(
		std::pair<elfptr_t, int>(elfptr, addr_stack.size()));

	vsb = vexsb_from_elfaddr(vexlate, img, elfptr);
	assert (vsb != NULL);

	char emitstr[1024];
	sprintf(emitstr, "sb_%p", elfptr);
	f = vsb->emit(emitstr);
	assert (f && "FAILED TO EMIT FUNC??");

	f->dump();
	new_jmpaddr = (elfptr_t)do_func(
		f, gs->getCPUState()->getStateData());

	/* push fall through address if call */
	if (vsb->isCall())
		addr_stack.push((elfptr_t)vsb->getEndAddr());

	if (vsb->isSyscall()) {
		SyscallParams	sp(gs->getSyscallParams());
		uint64_t	sc_ret;
		sc_ret = sc.apply(sp);
		gs->setSyscallResult(sc_ret);
	}

	if (vsb->isReturn()) addr_stack.pop();

	/* next address to go to */
	if (new_jmpaddr) addr_stack.push(new_jmpaddr);

	return vsb;
}

/* use the JITer to go through everything */
static void runImg(VexXlate* vexlate, ElfImg* img, GuestState* gs)
{
	std::string		exit_name("exit");
	guestptr_t		exit_ptr;

	/* libc hack. if we call the exit function, bail out */
	exit_ptr = gs->name2guest(exit_name);
	assert (exit_ptr && "Could not find exit pointer. What is this?");

	/* top of address stack is executed */
	addr_stack.push(img->getEntryPoint());
	while (!addr_stack.empty()) {
		VexSB	*sb;

		if ((elfptr_t)addr_stack.top() == (elfptr_t)exit_ptr) {
			printf("Exit call. Anthony fix this. Exitcode=%d",
				gs->getExitCode());
			printf("PID=%d\n", getpid());
			return;
		}

		sb = runNextVexSB(vexlate, img, gs);
		if (!sb) break;
		delete sb;
	}
}

int main(int argc, char* argv[])
{
	VexXlate	vexlate;
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

	/* XXX need to fix ownership of module--
	 * exe engine deletes it now! */
	gs = new GuestState(img);
	theGuestState = gs;
	theGenLLVM = new GenLLVM(gs);
	theVexHelpers = new VexHelpers();

	EngineBuilder	eb(theGenLLVM->getModule());
	std::string	err_str;

	eb.setErrorStr(&err_str);
	theExeEngine = eb.create();
	theVexHelpers->bindToExeEngine(theExeEngine);
	std::cout << err_str << std::endl;
	assert (theExeEngine && "Could not make exe engine");

	runImg(&vexlate, img, gs);

	printf("\nTRACE COMPLETE.\n");

	return 0;
}