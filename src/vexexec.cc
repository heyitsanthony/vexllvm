#include <llvm/IR/Value.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include "guestptimg.h"

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
#include "vexcpustate.h"
#include "guest.h"
#include "vexsb.h"
#include "vexxlate.h"
#include "vexexec.h"
#include "vexhelpers.h"
#include "guest.h"
#include "elfimg.h"
#include "jitengine.h"

extern "C" {
#include <valgrind/libvex_guest_amd64.h>
}

using namespace llvm;

VexExec* VexExec::exec_context = NULL;

#define TRACE_MAX	500

void VexExec::setupStatics(Guest* in_gs)
{
	const char* saveas;

	if (!theGenLLVM) theGenLLVM = std::make_unique<GenLLVM>(*in_gs);
	if (!theVexHelpers) theVexHelpers = VexHelpers::create(in_gs->getArch());

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

const VexSB* VexExec::getCachedVSB(guest_ptr p) const
{
	return jit_cache->getVSB(
		gs->getMem()->getHostPtr(p),
		p);
}

VexExec::~VexExec() {}

VexExec::VexExec(Guest* in_gs, std::shared_ptr<VexXlate> in_xlate)
: gs(in_gs)
, next_addr(0)
, sb_executed_c(0)
, exited(false)
, trace_c(0)
, save_core(getenv("VEXLLVM_CORE") != NULL)
, xlate(in_xlate)
{
	const char			*env_str;
	std::unique_ptr<JITEngine>	je(std::make_unique<JITEngine>());

	theVexHelpers->moveToJITEngine(*je);
	if (!xlate) xlate = std::make_shared<VexXlate>(gs->getArch());

	jit_cache = std::make_unique<VexJITCache>(xlate, std::move(je));
	if (getenv("VEXLLVM_VSB_MAXCACHE")) {
		jit_cache->setMaxCache(atoi(getenv("VEXLLVM_VSB_MAXCACHE")));
	}

	sc = Syscalls::create(gs);

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

	if (to_flush.first.o) {
		jit_cache->flush(
			guest_ptr(to_flush.first.o - MAX_CODE_BYTES),
			guest_ptr(to_flush.second.o + MAX_CODE_BYTES));
		to_flush = std::make_pair(guest_ptr(0), guest_ptr(0));
	}

	/* trying to jump to the null pointer is always a bad idea */
	if (elfptr.o == 0) {
		fprintf(stderr, "[VEXLLVM] Error: Trying to jump to NULL\n");
		return NULL;
	}

	vsb = getSBFromGuestAddr(elfptr);
	if (vsb == NULL) return NULL;

	new_jmpaddr = doVexSB(vsb);

	/* check for special exits */
	/* NOTE: we set the exit type *after* we process so that
	 * klee-mc will work */
	exit_type = gs->getVexState()->getExitType();
	switch(exit_type) {
	case GE_IGNORE:
		break;
	case GE_CALL:
		/* push fall through address if call */
		call_depth++;
		break;
	case GE_RETURN:
		call_depth--;
		break;
	case GE_INT:
	case GE_SYSCALL:
		doSysCall(vsb);
		if (exited) {
			gs->getVexState()->setExitType(GE_IGNORE);
			return NULL;
		}
		break;
	case GE_SIGTRAP:
		doTrap(vsb);
		if (exited) {
			gs->getVexState()->setExitType(GE_IGNORE);
			return NULL;
		}
		break;
	case GE_EMWARN:
		std::cerr << "[VEXLLVM] VEX Emulation warning!?"
			<< std::endl;
		break;
	default:
		std::cerr << "[VEXLLVM] UNKNOWN EXIT: " << exit_type << '\n';
		assert (0 == 1 && "SPECIAL EXIT TYPE");
	}

	gs->getVexState()->setExitType(GE_IGNORE);

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

	hostptr = gs->getMem()->getHostPtr(elfptr);
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
			std::cerr
				<< "[VEXLLVM] Tried to run non-exec memory @"
				<< (void*)elfptr.o
				<< "\n";
			return NULL;
		}
		if (m.cur_prot & PROT_WRITE) {
			m.cur_prot = m.req_prot & ~PROT_WRITE;
			mprotect(
				gs->getMem()->getHostPtr(m.offset),
				m.length,
				m.cur_prot);
			gs->getMem()->recordMapping(m);
		}
	}

	/* compile it + get vsb */
	vsb = jit_cache->getVSB(hostptr, elfptr);
	if (vsb == NULL) {
		uint8_t	buf[16];

		fprintf(stderr,
			"Could not get VSB for %p. Bad decode?\n",
			(void*)elfptr.o);
		gs->getMem()->memcpy(buf, elfptr, 16);
		for (unsigned i = 0; i < 16; i++)
			fprintf(stderr, "0x%02x ", buf[i]);
		fprintf(stderr, "\n");
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

	new_ip = ((vexauxfunc_t)(func_ptr))
		(gs->getCPUState()->getStateData(), aux);
	sb_executed_c++;

	return new_ip;
}

guest_ptr VexExec::doVexSB(VexSB* vsb)
{
	vexfunc_t		func_ptr;
	guest_ptr		new_ip;

	func_ptr = jit_cache->getCachedFPtr(vsb->getGuestAddr());
	assert (func_ptr != NULL);

	new_ip = func_ptr(gs->getCPUState()->getStateData());
	sb_executed_c++;

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

	next_addr = gs->getCPUState()->getPC();
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
	to_flush = std::make_pair(begin, end);
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

		if (exec_context->save_core)
			exec_context->gs->toCore();

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

		if (exec_context->save_core)
			exec_context->gs->toCore();

		exit(1);
	}
}

void VexExec::setSyscalls(std::unique_ptr<Syscalls> in_sc)
{
	sc = std::move(in_sc);
}
