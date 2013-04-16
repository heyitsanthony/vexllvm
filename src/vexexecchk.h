#ifndef VEXEXECCHK_H
#define VEXEXECCHK_H

#include "vexexec.h"

class PTImgChk;
class SyscallsMarshalled;
class VexXlate;

class VexExecChk : public VexExec
{
friend class VexExec;
public:
	virtual ~VexExecChk(void) {}

protected:
	VexExecChk(PTImgChk* gs, VexXlate* vx = NULL);

	virtual guest_ptr doVexSB(VexSB* vsb);
	virtual void doSysCall(VexSB* vsb);
	void doSysCallCore(VexSB* vsb);
	void verifyBlockRun(VexSB* vsb);
	void stepSysCall(VexSB* vsb);
	void dumpShadow(VexSB* vsb);

	PTImgChk		*cross_check;
private:
	bool		hit_syscall;
	bool		is_deferred;
	guest_ptr	deferred_bound_start;
	guest_ptr	deferred_bound_end;
};

#endif
