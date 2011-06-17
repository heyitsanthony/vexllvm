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
#include "guest.h"
#include "elfimg.h"
#include "vexsb.h"
#include "vexxlate.h"
#include "vexexec.h"
#include "vexhelpers.h"
#include "guest.h"
#include "memlog.h"

extern "C" {
#include <valgrind/libvex_guest_amd64.h>
}

using namespace llvm;

VexExec* VexExec::exec_context = NULL;

#define TRACE_MAX	500

void VexExec::setupStatics(Guest* in_gs)
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

VexExec::VexExec(Guest* in_gs)
: gs(in_gs)
, memory_log(getenv("VEXLLVM_LAST_STORE") ? new MemLog() : NULL)
, sb_executed_c(0)
, exited(false)
, trace_c(0)
{
	EngineBuilder	eb(theGenLLVM->getModule());
	ExecutionEngine	*exeEngine;
	std::string	err_str;
	const char	*env_str;

	eb.setErrorStr(&err_str);
	exeEngine = eb.create();
	assert (exeEngine && "Could not make exe engine");

	/* XXX need to fix ownership of module exe engine deletes it now! */
	theVexHelpers->bindToExeEngine(exeEngine);

	jit_cache = new VexJITCache(new VexXlate(), exeEngine);
	if (getenv("VEXLLVM_VSB_MAXCACHE")) {
		jit_cache->setMaxCache(atoi(getenv("VEXLLVM_VSB_MAXCACHE")));
	}

	sc = new Syscalls(gs);

	dump_current_state = (getenv("VEXLLVM_DUMP_STATES")) ? true : false;

	env_str = getenv("VEXLLVM_DISPATCH_TRACE");
	if (env_str != NULL) {
		if (strcmp(env_str, "stderr") == 0)  trace_conf = TRACE_STDERR;
		else if (strcmp(env_str, "log") == 0) trace_conf = TRACE_LOG;
	} else {
		trace_conf = TRACE_OFF;
	}

	//TODO: apply the protection changes to guard against code patches
}

const VexSB* VexExec::doNextSB(void)
{
	elfptr_t	elfptr;
	elfptr_t	new_jmpaddr;
	VexSB		*vsb;
	GuestExitType	exit_type;

	elfptr = addr_stack.top();

	if (trace_conf != TRACE_OFF) {
		if (trace_conf == TRACE_LOG) {
			if (trace_c > TRACE_MAX)
				trace.pop_front();
			else
				trace_c++;
			trace.push_back(std::pair<void*, int>(
				elfptr, addr_stack.size()));
		} else {
			std::cerr
				<< "[VEXLLVM] dispatch: "
				<< elfptr
				<< " (depth=" << addr_stack.size()
				<< ")\n";
		}
	}

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

	if (vsb->isReturn() && !addr_stack.empty()) {
		/* why are we tracking the stack like this... seems
		   weird to me, plus it means that the sp rewrites the
		   loader does don't work */
		addr_stack.pop();
		if(addr_stack.empty())
			addr_stack.push(new_jmpaddr);
	}


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
	GuestMem::Mapping		m;
	bool				found;

	hostptr = (void*)(gs->addr2Host((uint64_t)elfptr));

	/* XXX recongnize library ranges */
	if (hostptr == NULL) hostptr = elfptr;

	if (exit_addrs.count((elfptr_t)elfptr)) {
		exited = true;
		exit_code = gs->getExitCode();
		return NULL;
	}

	found = gs->getMem()->lookupMapping(hostptr, m);

	/* assuming !found means its ok... but that's not totally true */
	/* XXX, when is !found bad? */
	/* Disable writes to a page that is about to be executed.
	 * If the code is self-modifying, the disabled write will 
	 * trigger a signal handler which will invalidate the page's
	 * old code. */
	if (found) {
		if (!(m.req_prot & PROT_EXEC)) {
			assert (false && "Trying to jump to non-code");
		}
		if (m.cur_prot & PROT_WRITE) {
			m.cur_prot = m.req_prot & ~PROT_WRITE;
			mprotect(m.offset, m.length, m.cur_prot);
			gs->getMem()->recordMapping(m);
		}
	}

	/* compile it + get vsb */
	vsb = jit_cache->getVSB(hostptr, (uint64_t)elfptr);
	jit_cache->getFPtr(hostptr, (uint64_t)elfptr);

	if (!vsb) fprintf(stderr, "Could not get VSB for %p\n", elfptr);
	assert (vsb && "Expected VSB");
	assert((!found || (void*)vsb->getEndAddr() < m.end()) &&
		"code spanned known page mappings");

	return vsb;
}

uint64_t VexExec::doVexSB(VexSB* vsb)
{
	VexGuestAMD64State	*state;
	vexfunc_t		func_ptr;
	uint64_t		new_ip;

	func_ptr = jit_cache->getCachedFPtr(vsb->getGuestAddr());
	assert (func_ptr != NULL);

	sb_executed_c++;

	/* TODO: pull out x86-ism */
	state  = (VexGuestAMD64State*)gs->getCPUState()->getStateData();

	if(memory_log) {
		memory_log->clear();
		new_ip = ((vexlogfunc_t)(func_ptr))(state, memory_log);
	} else {
		new_ip = func_ptr(state);
	}
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
	jit_cache->flush(
		(void*)((uintptr_t)begin - MAX_CODE_BYTES),
		(void*)((uintptr_t)end + MAX_CODE_BYTES));
}

/* this happens when executing JITed code. Be careful about not flushing
 * the code we're currently running... */
void VexExec::signalHandler(int sig, siginfo_t* si, void* raw_context)
{
	// struct ucontext* context = (ucontext*)raw_context;
	GuestMem		*mem;
	GuestMem::Mapping	m;
	bool			found;

	mem = exec_context->gs->getMem();
	found = mem->lookupMapping(si->si_addr, m);
	if (!found) {
		std::cerr << "Caught SIGSEGV but couldn't "
			<< "find a mapping to tweak @ \n"
			<< si->si_addr << std::endl;
		exit(1);
	}

	if ((m.req_prot & PROT_EXEC) && !(m.cur_prot & PROT_WRITE)) {
		m.cur_prot = m.req_prot & ~PROT_EXEC;
		mprotect(m.offset, m.length, m.cur_prot);
		mem->recordMapping(m);
		exec_context->flushTamperedCode(m.offset, m.end());
	} else {
		std::cerr << "Caught SIGSEGV but the mapping was"
			<< "a normal one... die! @ \n"
			<< si->si_addr << std::endl;
		exit(1);
	}
}
