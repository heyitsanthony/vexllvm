#ifndef VEXIFACE_H
#define VEXIFACE_H

#include <iostream>
#include <stdint.h>

extern "C" {
#include <valgrind/libvex.h>
#include <valgrind/libvex_ir.h>
extern void dispatch_asm_amd64(void);
}
#include "elfimg.h"

class VexSB;

/* vexlate! */
class VexXlate
{
public:
	VexXlate(Arch::Arch arch);
	virtual ~VexXlate();
	VexSB* xlate(const void* guest_bytes, uint64_t guest_addr);
	void dumpLog(std::ostream& os) const;

	enum VexXlateLogType {
		VX_LOG_NONE,
		VX_LOG_MEM,
		VX_LOG_STDERR
		/* XXX ADD MORE */ };


private:
	void loadLogType(void);

	VexControl		vc;
	VexArchInfo		vai_host;
	VexArchInfo		vai_guest;
	VexAbiInfo		vbi;
	VexArch			arch;
};

#endif
