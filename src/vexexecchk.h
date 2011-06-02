#ifndef VEXEXECCHK_H
#define VEXEXECCHK_H

#include "vexexec.h"

class PTImgChk;
class SyscallsMarshalled;

class VexExecChk : public VexExec
{
public:
	virtual ~VexExecChk(void) {}
	static VexExec* create(PTImgChk* gs);
protected:
	VexExecChk(PTImgChk* gs);

	virtual uint64_t doVexSB(VexSB* vsb);
	virtual void doSysCall(VexSB* vsb);
private:
	void dumpSubservient(VexSB* vsb);
	void verifyBlockRun(VexSB* vsb);

	bool		hit_syscall;
	bool		is_deferred;
	uintptr_t	deferred_bound_start;
	uintptr_t	deferred_bound_end;

	PTImgChk		*cross_check;
	SyscallsMarshalled	*sc_marshall;
};

#endif
