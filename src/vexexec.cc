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
#include "gueststateptimg.h"

using namespace llvm;

#define TRACE_MAX	500

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
: gs(in_gs), sb_executed_c(0), cross_check(NULL), trace_c(0), exited(false)
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
	dump_llvm = (getenv("VEXLLVM_DUMP_LLVM")) ? true : false;
	if(getenv("VEXLLVM_CROSS_CHECK")) {
		cross_check = dynamic_cast<GuestStatePTImg*>(in_gs);
	}
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
			std::cerr << "[VEXLLVM] VEX Emulation warning!?" << std::endl;
			addr_stack.push(new_jmpaddr);
			return vsb;
		} else
			assert (0 == 1 && "SPECIAL EXIT TYPE");
	}

	/* push fall through address if call */
	if (vsb->isCall())
		addr_stack.push((elfptr_t)vsb->getEndAddr());

	if (vsb->isSyscall()) {
		SyscallParams	sp(gs->getSyscallParams());
		uint64_t	sc_ret;
		sc_ret = sc->apply(sp);
		if (sc->isExit()) {
			exited = true;
			exit_code = sc_ret;
			return NULL;
		}
		gs->setSyscallResult(sc_ret);
	}

	if (vsb->isReturn() && !addr_stack.empty())
		addr_stack.pop();

	/* next address to go to */
	if (	!(vsb->isReturn() && addr_stack.empty()) &&
		new_jmpaddr) addr_stack.push(new_jmpaddr);

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

	vsb = vexsb_dc.get(elfptr);
	if (vsb) return vsb;

	it = vexsb_cache.find(elfptr);
	if (it != vexsb_cache.end()) {
		vsb = (*it).second;
		goto update_dc;
	}

	if (exit_addrs.count((elfptr_t)elfptr)) {
		exited = true;
		exit_code = gs->getExitCode();
		return NULL;
	}

	vsb = vexlate->xlate(hostptr, (uint64_t)elfptr);
	vexsb_cache[elfptr] = vsb;

update_dc:
	vexsb_dc.put(elfptr, vsb);
	return vsb;
}

uint64_t VexExec::doVexSB(VexSB* vsb)
{
	vexfunc_t 			func_ptr;
	jit_map::const_iterator		it;

	func_ptr = (vexfunc_t)jit_dc.get((void*)vsb);
	if (func_ptr != NULL) goto hit_func;

	it = jit_cache.find(vsb);
	if (it != jit_cache.end()) {
		func_ptr = ((*it).second);
		goto miss_func;
	}	
	
	/* not in caches, generate */
	Function	*f;
	char		emitstr[1024];

	sprintf(emitstr, "sb_%p", (void*)vsb->getGuestAddr());
	f = vsb->emit(emitstr);
	assert (f && "FAILED TO EMIT FUNC??");

	if(dump_llvm)
		f->dump();

	func_ptr = (vexfunc_t)exeEngine->getPointerToFunction(f);
	assert (func_ptr != NULL && "Could not JIT");
	jit_cache[vsb] = func_ptr;

miss_func:
	jit_dc.put((void*)vsb, (vexfunc_t*)func_ptr);
	
hit_func:
	sb_executed_c++;
	VexGuestAMD64State* state = (VexGuestAMD64State*)gs->getCPUState()->getStateData();
	uint64_t r = func_ptr(state);
	std::cerr << "yo: " << (void*)vsb->getGuestAddr() << "-"  << (void*)vsb->getEndAddr() << " => "<< (void*)r << std::endl;
	state->guest_RIP = r;
	if(cross_check) {
		bool matched = cross_check->continueWithBounds(
			vsb->getGuestAddr(), vsb->getEndAddr(), *state);
			
		if(!matched && (r < vsb->getGuestAddr() || r >= vsb->getEndAddr())) {
			dumpSubservient(vsb);
		}
	}
	return r;
}
void VexExec::dumpSubservient(VexSB* vsb) {
	const VexGuestAMD64State& state = *(VexGuestAMD64State*)gs->getCPUState()->getStateData();
	std::cerr << "found divergence running block @ " << (void*)vsb->getGuestAddr() << std::endl;
	std::cerr << "original block end was @ " << (void*)vsb->getEndAddr() << std::endl;
	cross_check->printTraceStats(std::cerr);
	std::cerr << "PTRACE state" << std::endl;
	cross_check->printSubservient(std::cerr, &state);
	std::cerr << "VEXLLVM state" << std::endl;
	gs->print(std::cerr);
	std::cerr << "VEX IR" << std::endl;
	vsb->print(std::cerr);
	std::cerr << "PTRACE stack" << std::endl;
	cross_check->stackTraceSubservient(std::cerr);
	//if you want to keep going anyway, stop checking
	cross_check = NULL;
	exit(1);
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
	vexlate->dumpLog(os);
	sc->print(os);
}
