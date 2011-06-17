#ifndef VEXJITCACHE_H
#define VEXJITCACHE_H

#include "vexfcache.h"

namespace llvm {
class ExecutionEngine;
}

typedef uint64_t(*vexfunc_t)(void* /* guest cpu state */);
typedef std::map<uint64_t, vexfunc_t> jit_map;

/* if an auxiliary function is enabled, then this
   is the real type of the function.  this allows vexexec to pass
   the auxiliary  state into the translated code.
   Ideally, we should probably be hijacking the guest cpu state by
   expanding the tail. */
typedef uint64_t(*vexauxfunc_t)(
	void* /* guest cpu state */,
	void* /* auxiliary state */);


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
	virtual void flush(void);
	virtual void flush(void* begin, void* end);
private:
	llvm::ExecutionEngine	*exeEngine;
	jit_map			jit_cache;
	DirectCache<vexfunc_t>	jit_dc;
};

#endif
