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

class Guest;
class Syscalls;
class VexXlate;
class VexSB;
class SyscallParams;

namespace llvm
{
class ExecutionEngine;
class Function;
}


typedef std::list<std::pair<void*, int /* depth */> > vexexec_traces;
typedef std::stack<void*> vexexec_addrs;
typedef std::set<void*> exit_func_set;

class VexExec
{
public:
	template <class T, class U>
	static T* create(U* in_gs)
	{
		T	*ve;

		setupStatics(in_gs);

		ve = new T(in_gs);
		if (ve->getGuest() == NULL) {
			delete ve;
			return NULL;
		}

		return ve;
	}

	virtual ~VexExec(void);
	const Guest* getGuest(void) const { return gs; }
	const vexexec_addrs& getAddrStack(void) const { return addr_stack; }
	const vexexec_traces& getTraces(void) const { return trace; }
	void run(void);
	void dumpLogs(std::ostream& os) const;
	unsigned int getSBExecutedCount(void) const { return sb_executed_c; }

protected:
	VexExec(Guest* gs);
	virtual uint64_t doVexSB(VexSB* vsb);
	uint64_t doVexSBAux(VexSB* vsb, void* aux);

	virtual void doSysCall(VexSB* vsb);
	void doSysCall(VexSB* vsb, SyscallParams& sp);
	static void setupStatics(Guest* in_gs);

	Guest		*gs;
	Syscalls	*sc;
	VexFCache	*f_cache;

private:
	VexSB* getSBFromGuestAddr(void* elfptr);
	const VexSB* doNextSB(void);
	
	void runAddrStack(void);

	VexJITCache		*jit_cache;
	static VexExec		*exec_context;
	static void signalHandler(int sig, siginfo_t* si, void* raw_context);
	void flushTamperedCode(void* start, void* end);

	vexexec_addrs	addr_stack;

	/* stats */
	unsigned int	sb_executed_c;

	exit_func_set		exit_addrs;

	/* dump current state before executing BB */
	/* defined by env var VEXLLVM_DUMP_STATES */
	bool		dump_current_state;

	bool		exited;
	int		exit_code;

	enum TraceConf { TRACE_OFF, TRACE_LOG, TRACE_STDERR };
	TraceConf	trace_conf;
	vexexec_traces	trace;
	unsigned int	trace_c;
};

#endif
