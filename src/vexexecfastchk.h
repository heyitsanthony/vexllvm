#ifndef VEXEXECFASTCHK_H
#define VEXEXECFASTCHK_H

#include "vexexecchk.h"

class PTImgChk;

class VexExecFastChk : public VexExecChk
{
friend class VexExec;
public:
	virtual ~VexExecFastChk(void) {}
protected:
	VexExecFastChk(PTImgChk* gs);

	virtual uint64_t doVexSB(VexSB* vsb);
	virtual void doSysCall(VexSB* vsb);
private:
	unsigned int	pt_sc_req_c;	/* syscall opcodes ptrace seen */
	unsigned int	pt_sc_done_c; /* syscall opcodes ptrace retired */
	unsigned int	crashed_pt_sc_req;
};

#endif
