#ifndef VEXEXECCHK_H
#define VEXEXECCHK_H

#include "vexexec.h"

class PTImgChk;

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

private:
	void compareWithSubservient(VexSB* vsb);

	PTImgChk* cross_check;
};

#endif
