#ifndef VEXEXEC_H
#define VEXEXEC_H

#include <stdint.h>
#include <set>
#include <map>
#include <stack>
#include <list>
#include <vector>
#include <iostream>
#include "directcache.h"

class GuestState;
class Syscalls;
class VexXlate;
class VexSB;

namespace llvm
{
class ExecutionEngine;
class Function;
}


typedef uint64_t(*vexfunc_t)(void* /* guest cpu state */);

typedef std::list<std::pair<void*, int /* depth */> > vexexec_traces;
typedef std::stack<void*> vexexec_addrs;
typedef std::map<void* /* guest addr */, VexSB*> vexsb_map;
typedef std::map<VexSB*, vexfunc_t> jit_map;
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
		if (ve->getGuestState() == NULL) {
			delete ve;
			return NULL;
		}

		return ve;
	}

	virtual ~VexExec(void);
	const GuestState* getGuestState(void) const { return gs; }
	const vexexec_addrs& getAddrStack(void) const { return addr_stack; }
	const vexexec_traces& getTraces(void) const { return trace; }
	void run(void);
	void dumpLogs(std::ostream& os) const;
	unsigned int getSBExecutedCount(void) const { return sb_executed_c; }

protected:
	VexExec(GuestState* gs);
	virtual uint64_t doVexSB(VexSB* vsb);
	virtual void doSysCall(VexSB* vsb);
	static void setupStatics(GuestState* in_gs);

	GuestState		*gs;
	Syscalls		*sc;
private:
	vexfunc_t getSBFuncPtr(VexSB* vsb);
	VexSB* getSBFromGuestAddr(void* elfptr);
	const VexSB* doNextSB(void);
	
	void runAddrStack(void);
	void loadExitFuncAddrs(void);
	void glibcLocaleCheat(void);

	VexXlate		*vexlate;
	llvm::ExecutionEngine	*exeEngine;

	vexexec_addrs	addr_stack;
	vexexec_traces	trace;

	/* stats */
	unsigned int	sb_executed_c;

	vexsb_map		vexsb_cache;
	DirectCache<VexSB>	vexsb_dc;

	jit_map			jit_cache;
	DirectCache<vexfunc_t>	jit_dc;
	exit_func_set		exit_addrs;

	/* dump current state before executing BB */
	/* defined by env var VEXLLVM_DUMP_STATES */
	bool		dump_current_state;
	bool		dump_llvm;

	unsigned int	trace_c;
	bool		exited;
	int		exit_code;
};

#endif
