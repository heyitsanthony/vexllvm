#ifndef VEXIFACE_H
#define VEXIFACE_H

#include <iostream>
#include <stdint.h>

extern "C" {
#include <valgrind/libvex.h>
#include <valgrind/libvex_ir.h>
extern void dispatch_asm_amd64(void);
}

class VexSB;

/* vexlate! */
class VexXlate
{
public:
	VexXlate();
	virtual ~VexXlate();
	VexSB* xlate(const void* guest_bytes, uint64_t guest_addr);
	void dumpLog(std::ostream& os) const;
private:
	VexControl		vc;
	VexArchInfo		vai_amd64;
	VexAbiInfo		vbi;
};

#endif
