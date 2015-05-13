#ifndef VEXFCACHE_H
#define VEXFCACHE_H

#include <iostream>
#include <map>
#include <memory>

#include "arch.h"
#include "directcache.h"

namespace llvm
{
	class Function;
}

class VexSB;
class VexXlate;

typedef std::map<guest_ptr, VexSB*> vexsb_map;
typedef std::map<guest_ptr, llvm::Function*> func_map;

// function cache of vsb's */
class VexFCache
{
public:
	/* makes own vexxlate */
	VexFCache(Arch::Arch arch);
	/* claims ownership of vexxlate */
	VexFCache(std::shared_ptr<VexXlate> vexxlate);

	virtual ~VexFCache(void);

	llvm::Function* genFunctionByVSB(VexSB* vsb);

	/* do not try to free anything returned here!!! */
	VexSB* 		getVSB(void* hostptr, guest_ptr guest_addr);
	llvm::Function*	getFunc(void* hostptr, guest_ptr guest_addr);

	VexSB* getCachedVSB(guest_ptr guest_addr);
	llvm::Function* getCachedFunc(guest_ptr guest_addr);

	virtual void evict(guest_ptr guest_addr);
	void dumpLog(std::ostream& os) const;
	virtual void setMaxCache(unsigned int x);
	virtual void flush(void);
	virtual void flush(guest_ptr begin, guest_ptr end);

	unsigned int size(void) const { return vexsb_cache.size(); }
protected:
	unsigned int getMaxCache(void) const { return max_cache_ents; }
	func_map::iterator funcBegin() { return func_cache.begin(); }
	func_map::iterator funcEnd() { return func_cache.end(); }
	func_map::iterator funcBegin(guest_ptr b) {
		return func_cache.lower_bound(b); }
	func_map::iterator funcEnd(guest_ptr e) {
		return func_cache.upper_bound(e); }

	guest_ptr selectVictimAddress(void) const;
private:
	VexSB* allocCacheVSB(void* hostptr, guest_ptr guest_addr);

	bool			dump_llvm;

	std::shared_ptr<VexXlate>	xlate;
	unsigned int			max_cache_ents;

	vexsb_map		vexsb_cache;
	DirectCache<VexSB>	vexsb_dc;

	func_map			func_cache;
	DirectCache<llvm::Function>	func_dc;
};

#endif
