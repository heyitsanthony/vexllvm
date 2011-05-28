#ifndef VEXEXECCHK_H
#define VEXEXECCHK_H

#include "ptimgchk.h"
#include "vexexec.h"

class VexExecChk : public VexExec
{
public:
	virtual ~VexExecChk(void) {}
	static VexExec* create(PTImgChk* gs);
protected:
	VexExecChk(PTImgChk* gs)
	: is_deferred(false), VexExec(gs)
	{
		cross_check = (getGuestState()) ? gs : NULL;
	}

	virtual uint64_t doVexSB(VexSB* vsb);
	virtual void doSysCall(void);
	virtual void handlePostSyscall(VexSB* vsb, uint64_t);
private:
	void dumpSubservient(VexSB* vsb);

	bool		is_deferred;
	uintptr_t	deferred_bound_start;
	uintptr_t	deferred_bound_end;

	PTImgChk	*cross_check;
};

#endif
