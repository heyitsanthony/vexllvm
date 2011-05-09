#ifndef VEXEXEC_H
#define VEXEXEC_H

#include <stdint.h>
#include <map>
#include <stack>
#include <list>

class GuestState;
class Syscalls;
class VexXlate;
class VexSB;

namespace llvm
{
class ExecutionEngine;
class Function;
}

typedef std::list<std::pair<void*, int /* depth */> > vexexec_traces;
typedef std::stack<void*> vexexec_addrs;

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
	VexSB* doNextSB(void);
	uint64_t doFunc(llvm::Function* f);

	VexXlate		*vexlate;
	llvm::ExecutionEngine	*exeEngine;
	GuestState		*gs;
	Syscalls		*sc;

	vexexec_addrs	addr_stack;
	vexexec_traces	trace;

	/* stats */
	unsigned int	sb_executed_c;
};

#endif
