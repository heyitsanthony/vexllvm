#ifndef VEXFCACHE_H
#define VEXFCACHE_H

#include <iostream>
#include <map>
#include "directcache.h"
#include "elfimg.h"

namespace llvm
{
	class Function;
}

class VexSB;
class VexXlate;

typedef std::map<void* /* guest addr */, VexSB*> vexsb_map;
typedef std::map<void* /* guest addr */, llvm::Function*> func_map;

// function cache of vsb's */
class VexFCache
{
public:
	/* makes own vexxlate */
	VexFCache(Arch::Arch arch);
	/* claims ownership of vexxlate */
	VexFCache(VexXlate* vexxlate);

	virtual ~VexFCache(void);

	llvm::Function* genFunctionByVSB(VexSB* vsb);

	/* do not try to free anything returned here!!! */
	VexSB* 		getVSB(void* hostptr, uint64_t guest_addr);
	llvm::Function*	getFunc(void* hostptr, uint64_t guest_addr);

	VexSB* getCachedVSB(uint64_t guest_addr);
	llvm::Function* getCachedFunc(uint64_t guest_addr);

	virtual void evict(uint64_t guest_addr);
	void dumpLog(std::ostream& os) const;
	virtual void setMaxCache(unsigned int x);
	virtual void flush(void);
	virtual void flush(void* begin, void* end);
protected:
	unsigned int getMaxCache(void) const { return max_cache_ents; }
	func_map::iterator funcBegin() { return func_cache.begin(); }
	func_map::iterator funcEnd() { return func_cache.end(); }
	func_map::iterator funcBegin(void* b) {
		return func_cache.lower_bound(b); }
	func_map::iterator funcEnd(void* e) {
		return func_cache.upper_bound(e); }

	uint64_t selectVictimAddress(void) const;
private:
	VexSB* allocCacheVSB(void* hostptr, uint64_t guest_addr);

	bool			dump_llvm;

	VexXlate*		xlate;
	unsigned int		max_cache_ents;

	vexsb_map		vexsb_cache;
	DirectCache<VexSB>	vexsb_dc;

	func_map			func_cache;
	DirectCache<llvm::Function>	func_dc;
};

#endif
