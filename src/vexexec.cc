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

#include <inttypes.h>
#include <stdio.h>
#include <iostream>
#include <string>

#include "syscalls.h"
#include "genllvm.h"
#include "guestcpustate.h"
#include "gueststate.h"
#include "elfimg.h"
#include "vexsb.h"
#include "vexxlate.h"
#include "vexexec.h"
#include "vexhelpers.h"

using namespace llvm;

typedef uint64_t(*vexfunc_t)(void* /* guest cpu state */);

VexExec* VexExec::create(GuestState* in_gs)
{
	VexExec	*ve;

	if (!theGenLLVM) theGenLLVM = new GenLLVM(in_gs);
	if (!theVexHelpers) theVexHelpers = new VexHelpers();

	ve = new VexExec(in_gs);
	if (ve->getGuestState() == NULL) {
		delete ve;
		return NULL;
	}

	return ve;
}

VexExec::~VexExec()
{
	if (gs == NULL) return;
	delete vexlate;
	delete exeEngine;
	delete sc;
}

VexExec::VexExec(GuestState* in_gs)
: gs(in_gs), sb_executed_c(0)
{
	EngineBuilder	eb(theGenLLVM->getModule());
	std::string	err_str;

	eb.setErrorStr(&err_str);
	exeEngine = eb.create();
	assert (exeEngine && "Could not make exe engine");

	/* XXX need to fix ownership of module exe engine deletes it now! */
	theVexHelpers->bindToExeEngine(exeEngine);

	vexlate = new VexXlate();
	sc = new Syscalls();
}

VexSB* VexExec::doNextSB(void)
{
	elfptr_t	elfptr;
	llvm::Function	*f;
	elfptr_t	new_jmpaddr;
	VexSB		*vsb;
	GuestExitType	exit_type;

	elfptr = addr_stack.top();
	addr_stack.pop();
	trace.push_back(std::pair<void*, int>(elfptr, addr_stack.size()));

	vsb = getSBFromGuestAddr(elfptr);
	assert (vsb != NULL);

	char emitstr[1024];
	sprintf(emitstr, "sb_%p", elfptr);
	f = vsb->emit(emitstr);
	assert (f && "FAILED TO EMIT FUNC??");

	new_jmpaddr = (elfptr_t)doFunc(f);

	/* check for special exits */
	exit_type = gs->getCPUState()->getExitType();
	if (exit_type != GE_IGNORE) {
		if (exit_type == GE_EMWARN) {
			std::cerr << "VEX Emulation warning!?" << std::endl;
		} else
			assert (0 == 1 && "SPECIAL EXIT TYPE");
		gs->getCPUState()->setExitType(GE_IGNORE);
	}

	/* push fall through address if call */
	if (vsb->isCall())
		addr_stack.push((elfptr_t)vsb->getEndAddr());

	if (vsb->isSyscall()) {
		SyscallParams	sp(gs->getSyscallParams());
		uint64_t	sc_ret;
		sc_ret = sc->apply(sp);
		gs->setSyscallResult(sc_ret);
	}

	if (vsb->isReturn()) addr_stack.pop();

	/* next address to go to */
	if (new_jmpaddr) addr_stack.push(new_jmpaddr);

	return vsb;
}

VexSB* VexExec::getSBFromGuestAddr(void* elfptr)
{
	hostptr_t	hostptr;

	hostptr = (void*)(gs->addr2Host((uint64_t)elfptr));

	/* XXX recongnize library ranges */
	if (hostptr == NULL) hostptr = elfptr;

	return vexlate->xlate(hostptr, (uint64_t)elfptr);
}

uint64_t VexExec::doFunc(Function* f)
{
	/* don't forget to run it! */
	vexfunc_t func_ptr;
	
	func_ptr = (vexfunc_t)exeEngine->getPointerToFunction(f);
	assert (func_ptr != NULL && "Could not JIT");
//	fprintf(stderr, "doing func host=%p guest=%s\n", 
//		func_ptr, f->getNameStr().c_str());

	sb_executed_c++;
	return func_ptr(gs->getCPUState()->getStateData());
}

void VexExec::run(void)
{
	std::string		exit_name("exit");
	guestptr_t		exit_ptr;

	/* libc hack. if we call the exit function, bail out */
	exit_ptr = gs->name2guest(exit_name);
	assert (exit_ptr && "Could not find exit pointer. What is this?");

	/* top of address stack is executed */
	addr_stack.push(gs->getEntryPoint());
	while (!addr_stack.empty()) {
		VexSB	*sb;

		if ((elfptr_t)addr_stack.top() == (elfptr_t)exit_ptr) {
			printf("Exit call. Anthony fix this. Exitcode=%ld\n",
				gs->getExitCode());
			printf("PID=%d\n", getpid());
			return;
		}

		sb = doNextSB();
		if (!sb) break;
		delete sb;
	}
}

void VexExec::dumpLogs(std::ostream& os) const
{
	vexlate->dumpLog(os);
}
