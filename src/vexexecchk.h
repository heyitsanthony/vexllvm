#ifndef VEXEXECCHK_H
#define VEXEXECCHK_H

#include "vexexec.h"

class PTImgChk;
class SyscallsMarshalled;

class VexExecChk : public VexExec
{
friend class VexExec;
public:
	virtual ~VexExecChk(void) {}

protected:
	VexExecChk(PTImgChk* gs);

	virtual uint64_t doVexSB(VexSB* vsb);
	virtual void doSysCall(VexSB* vsb);
	void verifyBlockRun(VexSB* vsb);
	void stepSysCall(VexSB* vsb);
	void dumpSubservient(VexSB* vsb);

	PTImgChk		*cross_check;
	SyscallsMarshalled	*sc_marshall;

private:
	bool		hit_syscall;
	bool		is_deferred;
	uintptr_t	deferred_bound_start;
	uintptr_t	deferred_bound_end;
};

#endif
