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
#include <stdlib.h>
#include <iostream>
#include <string>

#include "Sugar.h"

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

	/* free cache */
	foreach (it, vexsb_cache.begin(), vexsb_cache.end()) {
		delete (*it).second;
	}

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

	dump_current_state = (getenv("VEXLLVM_DUMP_STATES")) ? true : false;
}

const VexSB* VexExec::doNextSB(void)
{
	elfptr_t	elfptr;
	elfptr_t	new_jmpaddr;
	VexSB		*vsb;
	GuestExitType	exit_type;

	elfptr = addr_stack.top();
	trace.push_back(std::pair<void*, int>(elfptr, addr_stack.size()));
	addr_stack.pop();

	vsb = getSBFromGuestAddr(elfptr);
	assert (vsb != NULL);

	new_jmpaddr = (void*)doVexSB(vsb);

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
	hostptr_t			hostptr;
	VexSB				*vsb;
	vexsb_map::const_iterator	it;

	hostptr = (void*)(gs->addr2Host((uint64_t)elfptr));

	/* XXX recongnize library ranges */
	if (hostptr == NULL) hostptr = elfptr;

	it = vexsb_cache.find(elfptr);
	if (it != vexsb_cache.end()) return (*it).second;

	vsb = vexlate->xlate(hostptr, (uint64_t)elfptr);
	vexsb_cache[elfptr] = vsb;

	return vsb;
}

uint64_t VexExec::doVexSB(VexSB* vsb)
{
	vexfunc_t 			func_ptr;
	jit_map::const_iterator		it;

	it = jit_cache.find(vsb);
	if (it != jit_cache.end()) {
		func_ptr = ((*it).second);
	} else {
		Function	*f;
		char		emitstr[1024];

		sprintf(emitstr, "sb_%p", (void*)vsb->getGuestAddr());
		f = vsb->emit(emitstr);
		assert (f && "FAILED TO EMIT FUNC??");
//		f->dump();
	
		func_ptr = (vexfunc_t)exeEngine->getPointerToFunction(f);
		assert (func_ptr != NULL && "Could not JIT");
		jit_cache[vsb] = func_ptr;
	}
//	fprintf(stderr, "doing func host=%p guest=%p\n", 
//		func_ptr, (void*)vsb->getGuestAddr());

	sb_executed_c++;
	return func_ptr(gs->getCPUState()->getStateData());
}

void VexExec::loadExitFuncAddrs(void)
{
	guestptr_t		exit_ptr;

	/* libc hack. if we call the exit function, bail out */
	exit_ptr = gs->name2guest("exit");
	assert (exit_ptr && "Could not find exit pointer. What is this?");
	exit_addrs.insert((void*)exit_ptr);

	exit_ptr = gs->name2guest("abort");
	assert (exit_ptr && "Could not find abort pointer. What is this?");
	exit_addrs.insert((void*)exit_ptr);

	exit_ptr = gs->name2guest("_exit");
	if (exit_ptr) exit_addrs.insert((void*)exit_ptr);
}

void VexExec::run(void)
{
	loadExitFuncAddrs();

	/* top of address stack is executed */
	addr_stack.push(gs->getEntryPoint());
	while (!addr_stack.empty()) {
		const VexSB	*sb;
		void		*top_addr;

		top_addr = addr_stack.top();
		if (exit_addrs.count((elfptr_t)top_addr)) {
			std::cerr	<< "Exit call. Anthony fix this. "
					<< "Exitcode=" << gs->getExitCode()
					<< std::endl;
			return;
		}

		if (dump_current_state) {
			std::cerr << "================BEFORE DOING "
				<< top_addr 
				<< std::endl;
			gs->print(std::cerr);
		}
		sb = doNextSB();
//		std::cerr << "================AFTER DOING " << top_addr 
//			<< std::endl;
//		gs->print(std::cerr);

		if (!sb) break;
	}
}

void VexExec::dumpLogs(std::ostream& os) const
{
	vexlate->dumpLog(os);
	sc->print(os);
}
