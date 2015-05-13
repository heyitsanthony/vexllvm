#ifndef VEXJITCACHE_H
#define VEXJITCACHE_H

#include "vexfcache.h"
#include "guestmem.h"
#include <memory>

typedef guest_ptr (*vexfunc_t)(void* /* guest cpu state */);
typedef std::map<guest_ptr, vexfunc_t> jit_map;

/* if an auxiliary function is enabled, then this
   is the real type of the function.  this allows vexexec to pass
   the auxiliary  state into the translated code.
   Ideally, we should probably be hijacking the guest cpu state by
   expanding the tail. */
typedef guest_ptr(*vexauxfunc_t)(
	void* /* guest cpu state */,
	void* /* auxiliary state */);

class JITEngine;

/**
 * JIT caches compiled functions as well as translated superblocks
 */
class VexJITCache : public VexFCache
{
public:
	VexJITCache(std::shared_ptr<VexXlate>, std::unique_ptr<JITEngine>);
	virtual ~VexJITCache(void);

	vexfunc_t getCachedFPtr(guest_ptr guest_addr);
	vexfunc_t getFPtr(void* host_addr, guest_ptr guest_addr);

	void evict(guest_ptr guest_addr) override;
	void flush(void) override;
	void flush(guest_ptr begin, guest_ptr end) override;
private:
	std::unique_ptr<JITEngine>	jit_engine;
	jit_map				jit_cache;
	DirectCache<vexfunc_t>		jit_dc;
};

#endif
