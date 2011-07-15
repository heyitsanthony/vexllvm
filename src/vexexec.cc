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

#include "syscall/syscalls.h"
#include "genllvm.h"
#include "guestcpustate.h"
#include "guest.h"
#include "vexsb.h"
#include "vexxlate.h"
#include "vexexec.h"
#include "vexhelpers.h"
#include "guest.h"
#include "elfimg.h"

extern "C" {
#include <valgrind/libvex_guest_amd64.h>
}

using namespace llvm;

VexExec* VexExec::exec_context = NULL;

#define TRACE_MAX	500

void VexExec::setupStatics(Guest* in_gs)
{
	const char* saveas;

	if (!theGenLLVM) theGenLLVM = new GenLLVM(in_gs);
	if (!theVexHelpers) theVexHelpers = new VexHelpers(in_gs->getArch());

	/* stupid hack to save right before execution */
	if (getenv("VEXLLVM_SAVE")) {
		fprintf(stderr, "Saving...\n");
		in_gs->save();
		exit(0);
	}

	if ((saveas = getenv("VEXLLVM_SAVEAS"))) {
		fprintf(stderr, "Saving as %s...\n", saveas);
		in_gs->save(saveas);
		exit(0);
	}
}

VexExec::~VexExec()
{
	if (gs == NULL) return;

	delete jit_cache;
	delete sc;

	if (owns_xlate) delete xlate;
}

VexExec::VexExec(Guest* in_gs, VexXlate* in_xlate)
: gs(in_gs)
, sb_executed_c(0)
, exited(false)
, trace_c(0)
, owns_xlate(in_xlate == NULL)
, xlate(in_xlate)
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

	if (!xlate) xlate = new VexXlate(gs->getArch());

	jit_cache = new VexJITCache(xlate, exeEngine);
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

#define MAX_CODE_BYTES 1024
const VexSB* VexExec::doNextSB(void)
{
	guest_ptr	elfptr;
	guest_ptr	new_jmpaddr;
	VexSB		*vsb;
	GuestExitType	exit_type;

	elfptr = next_addr;

	if (trace_conf != TRACE_OFF) {
		if (trace_conf == TRACE_LOG) {
			if (trace_c > TRACE_MAX)
				trace.pop_front();
			else
				trace_c++;
			trace.push_back(std::pair<guest_ptr, int>(
				elfptr, call_depth));
		} else {
			std::cerr
				<< "[VEXLLVM] dispatch: "
				<< elfptr
				<< " (depth=" << call_depth
				<< ")\n";
		}
	}

	if(to_flush.first.o) {
		jit_cache->flush(
			guest_ptr(to_flush.first.o - MAX_CODE_BYTES),
			guest_ptr(to_flush.second.o + MAX_CODE_BYTES));
		to_flush = std::make_pair(guest_ptr(0), guest_ptr(0));
	}

	vsb = getSBFromGuestAddr(elfptr);
	if (vsb == NULL) return NULL;

	new_jmpaddr = doVexSB(vsb);

	/* check for special exits */
	exit_type = gs->getCPUState()->getExitType();
	switch(exit_type) {
	case GE_IGNORE:
		gs->getCPUState()->setExitType(GE_IGNORE);
		break;
	case GE_CALL:
		/* push fall through address if call */
		gs->getCPUState()->setExitType(GE_IGNORE);
		call_depth++;
		break;
	case GE_RETURN:
		gs->getCPUState()->setExitType(GE_IGNORE);
		call_depth--;
		break;
	case GE_SYSCALL:
		gs->getCPUState()->setExitType(GE_IGNORE);
		doSysCall(vsb);
		if (exited) return NULL;
		break;
	case GE_EMWARN:
		gs->getCPUState()->setExitType(GE_IGNORE);
		std::cerr << "[VEXLLVM] VEX Emulation warning!?"
			<< std::endl;
		break;
	default:
		assert (0 == 1 && "SPECIAL EXIT TYPE");
	}

	/* next address to go to */
	next_addr = new_jmpaddr;

	return vsb;
}

void VexExec::doSysCall(VexSB* vsb)
{
	SyscallParams sp(gs->getSyscallParams());
	doSysCall(vsb, sp);
}

void VexExec::doSysCall(VexSB* vsb, SyscallParams& sp)
{
	uint64_t	sc_ret;

	sc_ret = sc->apply(sp);

	if (sc->isExit()) setExit(sc_ret);
	gs->setSyscallResult(sc_ret);
}

VexSB* VexExec::getSBFromGuestAddr(guest_ptr elfptr)
{
	void*				hostptr;
	VexSB				*vsb;
	vexsb_map::const_iterator	it;
	GuestMem::Mapping		m;
	bool				found;

	hostptr = gs->getMem()->getBase() + elfptr.o;

	if (exit_addrs.count(elfptr)) {
		setExit(gs->getExitCode());
		return NULL;
	}

	found = gs->getMem()->lookupMapping(elfptr, m);

	/* assuming !found means its ok... but that's not totally true */
	/* XXX, when is !found bad? */
	/* Disable writes to a page that is about to be executed.
	 * If the code is self-modifying, the disabled write will
	 * trigger a signal handler which will invalidate the page's
	 * old code. */
	if (found) {
		/* it be nice to factor some of this into guestmem */
		if (!(m.req_prot & PROT_EXEC)) {
			assert (false && "Trying to jump to non-code");
		}
		if (m.cur_prot & PROT_WRITE) {
			m.cur_prot = m.req_prot & ~PROT_WRITE;
			mprotect(gs->getMem()->getBase() + m.offset, m.length,
				m.cur_prot);
			gs->getMem()->recordMapping(m);
		}
	}

	/* compile it + get vsb */
	vsb = jit_cache->getVSB(hostptr, elfptr);
	if (vsb == NULL) {
		fprintf(stderr, "Could not get VSB for %p\n",  (void*)elfptr.o);
		return NULL;
	}

	jit_cache->getFPtr(hostptr, elfptr);

	assert((!found || vsb->getEndAddr() < m.end()) &&
		"code spanned known page mappings");

	return vsb;
}

guest_ptr VexExec::doVexSBAux(VexSB* vsb, void* aux)
{
	vexfunc_t		func_ptr;
	guest_ptr		new_ip;

	func_ptr = jit_cache->getCachedFPtr(vsb->getGuestAddr());
	assert (func_ptr != NULL);

	sb_executed_c++;

	sb_executed_c++;
	new_ip = ((vexauxfunc_t)(func_ptr))
		(gs->getCPUState()->getStateData(), aux);
	return new_ip;
}

guest_ptr VexExec::doVexSB(VexSB* vsb)
{
	vexfunc_t		func_ptr;
	guest_ptr		new_ip;

	func_ptr = jit_cache->getCachedFPtr(vsb->getGuestAddr());
	assert (func_ptr != NULL);

	sb_executed_c++;
	new_ip = func_ptr(gs->getCPUState()->getStateData());

	return new_ip;
}

void VexExec::run(void)
{
	beginStepping();
	while (stepVSB()) {
	}
}

void VexExec::beginStepping(void)
{
	struct sigaction sa;

	VexExec::exec_context = this;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_sigaction = &VexExec::signalHandler;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
   	sigaction(SIGSEGV, &sa, NULL);

	next_addr = guest_ptr(gs->getEntryPoint());
	call_depth = 0;
}

bool VexExec::stepVSB(void)
{
	const VexSB	*sb;
	guest_ptr	top_addr;

	if (exited) goto done_with_all;

	if (dump_current_state) {
		std::cerr << "================BEFORE DOING "
			<< (void*)next_addr.o
			<< std::endl;
		gs->print(std::cerr);
	}

	sb = doNextSB();
	if (sb) return true;

done_with_all:
	/* ran out of stuff to do, but didn't exit */
	if (!exited) return false;

	/* exited */
	std::cerr 
		<< "[VEXLLVM] Exit call. This should be fixed. "
		<< "Exitcode=" << exit_code
		<< std::endl;
	return false;
}

void VexExec::dumpLogs(std::ostream& os) const
{
	sc->print(os);
}

void VexExec::flushTamperedCode(guest_ptr begin, guest_ptr end)
{
	to_flush = std::make_pair((uintptr_t)begin, (uintptr_t)end);
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
	found = mem->lookupMapping(
		guest_ptr((char*)si->si_addr - mem->getBase()), m);
	if (!found) {
		std::cerr << "Caught SIGSEGV but couldn't "
			<< "find a mapping to tweak @ \n"
			<< si->si_addr << std::endl;
		exit(1);
	}
	if ((m.req_prot & PROT_EXEC) && !(m.cur_prot & PROT_WRITE)) {
		m.cur_prot = m.req_prot & ~PROT_EXEC;
		mprotect(mem->getBase() + m.offset, m.length, m.cur_prot);
		mem->recordMapping(m);
		exec_context->flushTamperedCode(m.offset, m.end());
	} else {
		std::cerr << "Caught SIGSEGV but the mapping was"
			<< "a normal one... die! @ \n"
			<< si->si_addr << std::endl;
		exit(1);
	}
}
