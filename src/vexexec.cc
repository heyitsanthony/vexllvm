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

VexExec* VexExec::exec_context = NULL;

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
	sc = new Syscalls(mappings, gs->getBinaryPath());

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
	VexSB				*vsb;
	vexsb_map::const_iterator	it;
	VexMem::Mapping			m;
	bool				found;

	hostptr = (void*)(gs->addr2Host((uint64_t)elfptr));

	/* XXX recongnize library ranges */
	if (hostptr == NULL) hostptr = elfptr;

	if (exit_addrs.count((elfptr_t)elfptr)) {
		exited = true;
		exit_code = gs->getExitCode();
		return NULL;
	}

	found = mappings.lookupMapping(hostptr, m);
	/* assuming !found means its ok... but that's not totally true */
	if (found) {
		if(!(m.req_prot & PROT_EXEC)) {
			assert(false && "Trying to jump to non-code");
		}
		if(m.cur_prot & PROT_WRITE) {
			m.cur_prot = m.req_prot & ~PROT_WRITE;
			mprotect(m.offset, m.length, m.cur_prot);
			mappings.recordMapping(m);
		}
	}

	/* compile it */
	jit_cache->getFPtr(hostptr, (uint64_t)elfptr);

	/* vsb should be cached still */
	vsb = jit_cache->getCachedVSB((uint64_t)elfptr);

	assert((!found || (void*)vsb->getEndAddr() < m.end()) && 
		"code spanned known page mappings");

	return vsb;
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
	struct sigaction sa;

	VexExec::exec_context = this;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_sigaction = &VexExec::signalHandler;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
   	sigaction(SIGSEGV, &sa, NULL);
   
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
	sc->print(os);
}

#define MAX_CODE_BYTES 1024
void VexExec::flushTamperedCode(void* begin, void* end)
{
	/* XXX flush code between begin and end */
	assert (0 == 1 && "STUB: FIXME FIXME");
}

/* these only occur while accessing executable mappings-- so it should 
 * be safe to evict anything in the jit cache (mappings only touch ...) */
void VexExec::signalHandler(int sig, siginfo_t* si, void* raw_context)
{
	// struct ucontext* context = (ucontext*)raw_context;
	VexMem::Mapping m;
	bool found = exec_context->mappings.lookupMapping(si->si_addr, m);
	if(!found) {
		std::cerr << "Caught SIGSEGV but couldn't " 
			<< "find a mapping to tweak @ \n" 
			<< si->si_addr << std::endl;
		exit(1);
	}

	if((m.req_prot & PROT_EXEC) && !(m.cur_prot & PROT_WRITE)) {
		m.cur_prot = m.req_prot & ~PROT_EXEC;
		mprotect(m.offset, m.length, m.cur_prot);
		exec_context->mappings.recordMapping(m);
		exec_context->flushTamperedCode(m.offset, m.end());
	} else {
		std::cerr << "Caught SIGSEGV but the mapping was" 
			<< "a normal one... die! @ \n" 
			<< si->si_addr << std::endl;
		exit(1);
	}
}
