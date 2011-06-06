#ifndef VEXJITCACHE_H
#define VEXJITCACHE_H

#include "vexfcache.h"

namespace llvm {
class ExecutionEngine;
}

typedef uint64_t(*vexfunc_t)(void* /* guest cpu state */);
typedef std::map<uint64_t, vexfunc_t> jit_map;

class VexJITCache : public VexFCache
{
public:
	VexJITCache(
		VexXlate* xlate, 
		llvm::ExecutionEngine *exeEngine);
	virtual ~VexJITCache(void);

	vexfunc_t getCachedFPtr(uint64_t guest_addr);
	vexfunc_t getFPtr(void* host_addr, uint64_t guest_addr);
	virtual void evict(uint64_t guest_addr);
private:
	llvm::ExecutionEngine	*exeEngine;
	jit_map			jit_cache;
	DirectCache<vexfunc_t>	jit_dc;
};

#endif
