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


#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "syscalls.h"
#include "vexxlate.h"
#include "elfimg.h"

extern "C" {
#include <valgrind/libvex.h>
#include <valgrind/libvex_ir.h>
#include <valgrind/libvex_guest_amd64.h>
};

#include "vexhelpers.h"
#include "vexsb.h"
#include "gueststate.h"
#include "guestcpustate.h"
#include "genllvm.h"

using namespace llvm;

static ExecutionEngine		*theExeEngine;

typedef uint64_t(*vexfunc_t)(void* /* guest state */);

std::list<elfptr_t>	trace;

static VexSB* vexsb_from_elfaddr(
	VexXlate* vexlate, ElfImg* img, elfptr_t elfptr)
{
	hostptr_t	hostptr;

	hostptr = img->xlateAddr(elfptr);

	std::cout << "GUEST ADDR=" << elfptr << std::endl;
//	std::cout << "ELFENT= " <<  elfptr << "<--> "
//		<< hostptr			
//		<< " =HOSTENT" <<std::endl;
	/* XXX recongnize library ranges */
	if (hostptr == NULL) hostptr = elfptr;
#if 0
	if (((uintptr_t)hostptr & 0xfff) == 0x3f0) { 
		std::cerr << "TRACE:" << std::endl;
		foreach(it, trace.begin(), trace.end()) {
			std::cerr << *it << std::endl;
		}
		exit(-1);
		
	}
#endif
	printf("xlating %p\n", hostptr);
	return vexlate->xlate(hostptr, (uint64_t)elfptr);
}

uint64_t do_func(Function* f, void* guest_ctx)
{
	/* don't forget to run it! */
	vexfunc_t func_ptr;
	
	fprintf(stderr, "JITing: %s\n", (f->getName()).data());
	func_ptr = (vexfunc_t)theExeEngine->getPointerToFunction(f);
	assert (func_ptr != NULL && "Could not JIT");

	fprintf(stderr, "Calling: %s (JIT=%p)\n", 
		(f->getName()).data(),
		func_ptr);
	return func_ptr(guest_ctx);
}

/* use the JITer to go through everything */
static void processFuncs(VexXlate* vexlate, ElfImg* img, GuestState* gs)
{
	std::stack<elfptr_t>	addr_stack;	/* holds elf addresses */
	Syscalls		sc;

	addr_stack.push(img->getEntryPoint());
	while (!addr_stack.empty()) {
		elfptr_t	elfptr;
		llvm::Function	*f;
		elfptr_t	new_jmpaddr;
		VexSB		*vsb;

		elfptr = addr_stack.top();
		addr_stack.pop();
		trace.push_back(elfptr);

		vsb = vexsb_from_elfaddr(vexlate, img, elfptr);
		assert (vsb != NULL);

		char emitstr[1024];
		sprintf(emitstr, "sb_%p", elfptr);
		printf("emitting %s\n", emitstr);
		f = vsb->emit(emitstr);
		printf ("%s emitted.\n", emitstr);
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

		delete vsb;
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

	img = ElfImg::create(argv[1]);
	if (img == NULL) {
		fprintf(stderr, "%s: Could not open ELF %s\n", 
			argv[0], argv[1]);
		return -2;
	}

	/* XXX need to fix ownership of module--
	 * exe engine deletes it now! */
	gs = new GuestState(img);
	theGenLLVM = new GenLLVM(gs);
	theVexHelpers = new VexHelpers();

	EngineBuilder	eb(theGenLLVM->getModule());
	std::string	err_str;

	eb.setErrorStr(&err_str);
	theExeEngine = eb.create();
	theVexHelpers->bindToExeEngine(theExeEngine);
	std::cout << err_str << std::endl;
	assert (theExeEngine && "Could not make exe engine");

	processFuncs(&vexlate, img, gs);

	printf("\nLLVEX.\n");

	return 0;
}