#ifndef VEXOP_H
#define VEXOP_H

extern "C" {
#include <valgrind/libvex.h>
#include <valgrind/libvex_ir.h>
}

const char* getVexOpName(IROp op);

#endif
