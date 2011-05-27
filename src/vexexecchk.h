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
	: VexExec(gs)
	{
		cross_check = (getGuestState()) ? gs : NULL;
	}

	virtual uint64_t doVexSB(VexSB* vsb);
	virtual void doSysCall(void);
	virtual void handlePostSyscall(VexSB* vsb, uint64_t);
private:
	void compareWithSubservient(VexSB* vsb);
	void dumpSubservient(VexSB* vsb);

	PTImgChk* cross_check;
};

#endif
