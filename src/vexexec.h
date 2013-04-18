#ifndef VEXEXEC_H
#define VEXEXEC_H

#include <sys/signal.h>
#include <stdint.h>
#include <set>
#include <map>
#include <stack>
#include <list>
#include <vector>
#include <iostream>
#include "vexjitcache.h"
#include "guestmem.h"
#include "guestcpustate.h"

class Guest;
class Syscalls;
class VexXlate;
class VexSB;
class SyscallParams;
class VexXlate;

namespace llvm
{
class ExecutionEngine;
class Function;
}


typedef std::list<std::pair<guest_ptr, int /* depth */> > vexexec_traces;
typedef std::stack<guest_ptr> vexexec_addrs;
typedef std::set<guest_ptr> exit_func_set;

class VexExec
{
public:
	template <class T, class U>
	static T* create(U* in_gs, VexXlate* in_xlate = NULL)
	{
		T	*ve;

		setupStatics(in_gs);

		ve = new T(in_gs, in_xlate);
		if (ve->getGuest() == NULL) {
			delete ve;
			return NULL;
		}
		ve->gs->getCPUState()->setPC(ve->gs->getEntryPoint());

		return ve;
	}

	virtual ~VexExec(void);
	const Guest* getGuest(void) const { return gs; }
	const vexexec_traces& getTraces(void) const { return trace; }
	void run(void);
	void dumpLogs(std::ostream& os) const;
	unsigned int getSBExecutedCount(void) const { return sb_executed_c; }
	const VexSB* getCachedVSB(guest_ptr p) const;

	void beginStepping(void);
	bool stepVSB(void);

	guest_ptr getNextAddr(void) const { return next_addr; }

	virtual void setSyscalls(Syscalls* in_sc);
protected:
	VexExec(Guest* gs, VexXlate* in_xlate = NULL);
	virtual guest_ptr doVexSB(VexSB* vsb);
	guest_ptr doVexSBAux(VexSB* vsb, void* aux);

	virtual void doSysCall(VexSB* vsb);
	virtual void doTrap(VexSB* vsb) {}
	void doSysCall(VexSB* vsb, SyscallParams& sp);
	static void setupStatics(Guest* in_gs);
	void setExit(int ec) { exited = true; exit_code = ec; }

	Guest		*gs;
	Syscalls	*sc;
	VexFCache	*f_cache;
	guest_ptr	next_addr;

private:
	VexSB*		getSBFromGuestAddr(guest_ptr elfptr);
	const VexSB*	doNextSB(void);

	VexJITCache		*jit_cache;
	static VexExec		*exec_context;
	static void signalHandler(int sig, siginfo_t* si, void* raw_context);
	void flushTamperedCode(guest_ptr start, guest_ptr end);

	int		call_depth;

	/* stats */
	unsigned int	sb_executed_c;

	/* dump current state before executing BB */
	/* defined by env var VEXLLVM_DUMP_STATES */
	bool		dump_current_state;

	bool		exited;
	int		exit_code;

	enum TraceConf { TRACE_OFF, TRACE_LOG, TRACE_STDERR };
	TraceConf	trace_conf;
	vexexec_traces	trace;
	unsigned int	trace_c;

	bool		owns_xlate;
	VexXlate	*xlate;
	std::pair<guest_ptr, guest_ptr>	to_flush;
};

#endif
