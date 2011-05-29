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
	: VexExec(gs),
	  hit_syscall(false), is_deferred(false)
	{
		cross_check = (getGuestState()) ? gs : NULL;
	}

	virtual uint64_t doVexSB(VexSB* vsb);
	virtual void doSysCall(void);
private:
	void dumpSubservient(VexSB* vsb);

	bool		hit_syscall;
	bool		is_deferred;
	uintptr_t	deferred_bound_start;
	uintptr_t	deferred_bound_end;

	PTImgChk	*cross_check;
};

#endif
