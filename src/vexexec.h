#ifndef VEXEXEC_H
#define VEXEXEC_H

#include <stdint.h>
#include <set>
#include <map>
#include <stack>
#include <list>
#include <vector>

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
	static VexExec* create(GuestState* gs);
	virtual ~VexExec(void);
	const GuestState* getGuestState(void) const { return gs; }
	const vexexec_addrs& getAddrStack(void) const { return addr_stack; }
	const vexexec_traces& getTraces(void) const { return trace; }
	void run(void);
	void dumpLogs(std::ostream& os) const;
	unsigned int getSBExecutedCount(void) const { return sb_executed_c; }
private:
	VexExec(GuestState* gs);
	VexSB* getSBFromGuestAddr(void* elfptr);
	const VexSB* doNextSB(void);
	uint64_t doVexSB(VexSB* vsb);
	void loadExitFuncAddrs(void);

	VexXlate		*vexlate;
	llvm::ExecutionEngine	*exeEngine;
	GuestState		*gs;
	Syscalls		*sc;

	vexexec_addrs	addr_stack;
	vexexec_traces	trace;

	/* stats */
	unsigned int	sb_executed_c;

	vexsb_map	vexsb_cache;
	jit_map		jit_cache;
	exit_func_set	exit_addrs;

	/* dump current state before executing BB */
	/* defined by env var VEXLLVM_DUMP_STATES */
	bool		dump_current_state;

	unsigned int	trace_c;
};

#endif
