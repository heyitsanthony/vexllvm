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
#include <sys/signal.h>
#include <sys/mman.h>
#include <ucontext.h>

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
#include "gueststateptimg.h"

using namespace llvm;

#define TRACE_MAX	500

void VexExec::setupStatics(GuestState* in_gs)
{
	if (!theGenLLVM) theGenLLVM = new GenLLVM(in_gs);
	if (!theVexHelpers) theVexHelpers = new VexHelpers();
}


VexExec::~VexExec()
{
	if (gs == NULL) return;

	delete jit_cache;
	delete sc;
}

VexExec::VexExec(GuestState* in_gs)
: gs(in_gs), sb_executed_c(0), trace_c(0), exited(false)
{
	EngineBuilder	eb(theGenLLVM->getModule());
	ExecutionEngine	*exeEngine;
	std::string	err_str;

	eb.setErrorStr(&err_str);
	exeEngine = eb.create();
	assert (exeEngine && "Could not make exe engine");

	/* XXX need to fix ownership of module exe engine deletes it now! */
	theVexHelpers->bindToExeEngine(exeEngine);

	jit_cache = new VexJITCache(new VexXlate(), exeEngine);
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
	
	if (trace_c > TRACE_MAX)
		trace.pop_front();
	else
		trace_c++;
	trace.push_back(std::pair<void*, int>(elfptr, addr_stack.size()));

	addr_stack.pop();

	vsb = getSBFromGuestAddr(elfptr);
	if (vsb == NULL) return NULL;

	new_jmpaddr = (void*)doVexSB(vsb);

	/* check for special exits */
	exit_type = gs->getCPUState()->getExitType();
	if (exit_type != GE_IGNORE) {
		gs->getCPUState()->setExitType(GE_IGNORE);
		if (exit_type == GE_EMWARN) {
			std::cerr << "[VEXLLVM] VEX Emulation warning!?" 
				<< std::endl;
			addr_stack.push(new_jmpaddr);
			return vsb;
		} else
			assert (0 == 1 && "SPECIAL EXIT TYPE");
	}

	/* push fall through address if call */
	if (vsb->isCall())
		addr_stack.push((elfptr_t)vsb->getEndAddr());

	if (vsb->isSyscall()) {
		doSysCall(vsb);
		if (exited) return NULL;
	}

	if (vsb->isReturn() && !addr_stack.empty())
		addr_stack.pop();

	/* next address to go to */
	if (	!(vsb->isReturn() && addr_stack.empty()) &&
		new_jmpaddr) addr_stack.push(new_jmpaddr);

	return vsb;
}

void VexExec::doSysCall(VexSB* vsb)
{
	SyscallParams	sp(gs->getSyscallParams());
	uint64_t	sc_ret;

	sc_ret = sc->apply(sp);
	if (sc->isExit()) {
		exited = true;
		exit_code = sc_ret;
	}
	gs->setSyscallResult(sc_ret);
}

VexSB* VexExec::getSBFromGuestAddr(void* elfptr)
{
	hostptr_t			hostptr;

	hostptr = (void*)(gs->addr2Host((uint64_t)elfptr));

	/* XXX recongnize library ranges */
	if (hostptr == NULL) hostptr = elfptr;

	if (exit_addrs.count((elfptr_t)elfptr)) {
		exited = true;
		exit_code = gs->getExitCode();
		return NULL;
	}

	/* compile it */
	jit_cache->getFPtr(hostptr, (uint64_t)elfptr);

	/* vsb should be cached still */
	return jit_cache->getCachedVSB((uint64_t)elfptr);
}

uint64_t VexExec::doVexSB(VexSB* vsb)
{
	VexGuestAMD64State* state;
	vexfunc_t	func_ptr;
	uint64_t	new_ip;

	func_ptr = jit_cache->getCachedFPtr(vsb->getGuestAddr());
	assert (func_ptr != NULL);

	sb_executed_c++;

	/* TODO: pull out x86-ism */
	state  = (VexGuestAMD64State*)gs->getCPUState()->getStateData();
	new_ip = func_ptr(state);
	state->guest_RIP = new_ip;

	return new_ip;
}

void VexExec::loadExitFuncAddrs(void)
{
	guestptr_t		exit_ptr;

	/* libc hack. if we call the exit function, bail out */
	exit_ptr = gs->name2guest("exit");
//	assert (exit_ptr && "Could not find exit pointer. What is this?");
	exit_addrs.insert((void*)exit_ptr);

	exit_ptr = gs->name2guest("abort");
//	assert (exit_ptr && "Could not find abort pointer. What is this?");
	exit_addrs.insert((void*)exit_ptr);

	exit_ptr = gs->name2guest("_exit");
	if (exit_ptr) exit_addrs.insert((void*)exit_ptr);

	exit_ptr = gs->name2guest("exit_group");
	if (exit_ptr) exit_addrs.insert((void*)exit_ptr);
}

/**
 * glibc requires some gettext stuff set, but since we're not using the ELF's
 * interp (because it'd be a lot of work!),  we're not getting some locale var
 * set-- fix = call uselocale and hope everything works out
 */
void VexExec::glibcLocaleCheat(void)
{
	guestptr_t	uselocale_ptr;
	void		*old_stack;

	uselocale_ptr = gs->name2guest("uselocale");
	if (!uselocale_ptr) return;

	gs->getCPUState()->setFuncArg(-1, 0);
	old_stack = gs->getCPUState()->getStackPtr();

	addr_stack.push((void*)uselocale_ptr);
	runAddrStack();
	addr_stack = vexexec_addrs();

	gs->getCPUState()->setStackPtr(old_stack);
}

void VexExec::run(void)
{
	loadExitFuncAddrs();
	glibcLocaleCheat();

	/* top of address stack is executed */
	addr_stack.push(gs->getEntryPoint());
	runAddrStack();
}

void VexExec::runAddrStack(void)
{
	while (!addr_stack.empty()) {
		const VexSB	*sb;
		void		*top_addr;

		top_addr = addr_stack.top();
		if (dump_current_state) {
			std::cerr << "================BEFORE DOING "
				<< top_addr 
				<< std::endl;
			gs->print(std::cerr);
		}

		sb = doNextSB();
		if (!sb) break;
	}

	if (exited) {
		std::cerr 	<< "[VEXLLVM] Exit call. Anthony fix this. "
				<< "Exitcode=" << exit_code
				<< std::endl;
	}
}

void VexExec::dumpLogs(std::ostream& os) const
{
	jit_cache->dumpLog(os);
	sc->print(os);
}