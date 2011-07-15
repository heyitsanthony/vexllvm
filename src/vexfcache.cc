#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <list>
#include "llvm/Function.h"

#include "Sugar.h"
#include "vexsb.h"
#include "vexxlate.h"
#include "vexfcache.h"

using namespace llvm;

VexFCache::VexFCache(Arch::Arch arch)
: xlate(new VexXlate(arch)),
  owns_xlate(true),
  max_cache_ents(~0) /* don't evict by default */
{
	dump_llvm = (getenv("VEXLLVM_DUMP_LLVM")) ? true : false;
}

VexFCache::VexFCache(VexXlate* in_xlate)
: xlate(in_xlate),
  owns_xlate(false),
  max_cache_ents(~0)
{
	assert (xlate != NULL);
	dump_llvm = (getenv("VEXLLVM_DUMP_LLVM")) ? true : false;
}

VexFCache::~VexFCache(void)
{
	flush();
	if (owns_xlate) delete xlate;
}

void VexFCache::setMaxCache(unsigned int x)
{
	if (x < max_cache_ents) flush();
	max_cache_ents = x;
}

/* XXX: be smarter; random eviction is stupid */
guest_ptr VexFCache::selectVictimAddress(void) const
{
	int		choice = rand() % vexsb_cache.size();
	guest_ptr	evict_addr(0);

	foreach (it, vexsb_cache.begin(), vexsb_cache.end()) {
		if (!choice) {
			evict_addr = it->second->getGuestAddr();
			break;
		}
		choice--;
	}

	assert (evict_addr && "0 evict addr?");
	return evict_addr;
}

VexSB* VexFCache::getVSB(void* hostptr, guest_ptr guest_addr)
{
	VexSB*	vsb;

	vsb = getCachedVSB(guest_addr);
	if (vsb) return vsb;

	return allocCacheVSB(hostptr, guest_addr);
}

VexSB* VexFCache::allocCacheVSB(void* hostptr, guest_ptr guest_addr)
{
	VexSB*	vsb;

	vsb = xlate->xlate(hostptr, guest_addr);
	if (vsb == NULL)
		return NULL;

	if (vexsb_cache.size() == max_cache_ents)
		evict(selectVictimAddress());
	assert (vexsb_cache.size() < max_cache_ents);

	vexsb_cache[guest_addr] = vsb;
	vexsb_dc.put(guest_addr, vsb);
	return vsb;
}

VexSB* VexFCache::getCachedVSB(guest_ptr guest_addr)
{
	VexSB				*vsb;
	vexsb_map::const_iterator	it;

	vsb = vexsb_dc.get(guest_addr);
	if (vsb) return vsb;

	it = vexsb_cache.find(guest_addr);
	if (it == vexsb_cache.end()) return NULL;

	vsb = (*it).second;
	vexsb_dc.put(guest_addr, vsb);
	return vsb;
}

Function* VexFCache::getCachedFunc(guest_ptr guest_addr)
{
	Function			*func;
	func_map::const_iterator	it;

	func = func_dc.get(guest_addr);
	if (func) return func;

	it = func_cache.find(guest_addr);
	if (it == func_cache.end()) return NULL;

	func = (*it).second;
	func_dc.put(guest_addr, func);
	return func;
}

Function* VexFCache::getFunc(void* hostptr, guest_ptr guest_addr)
{
	Function*	ret_f;
	VexSB*		vsb;

	ret_f = getCachedFunc(guest_addr);
	if (ret_f) return ret_f;

	vsb = getVSB(hostptr, guest_addr);
	if (vsb == NULL)
		return NULL;

	ret_f = genFunctionByVSB(vsb);
	return ret_f;
}

Function* VexFCache::genFunctionByVSB(VexSB* vsb)
{
	Function			*f;
	char				emitstr[512];

	/* not in caches, generate */
	sprintf(emitstr, "sb_%p", (void*)vsb->getGuestAddr().o);
	f = vsb->emit(emitstr);
	assert (f && "FAILED TO EMIT FUNC??");

	func_cache[vsb->getGuestAddr()] = f;
	func_dc.put(vsb->getGuestAddr(), f);

	if (dump_llvm) f->dump();

	return f;
}

void VexFCache::evict(guest_ptr guest_addr)
{
	VexSB		*vsb;
	Function	*func;

	if ((func = getCachedFunc(guest_addr)) != NULL) {
		func_cache.erase(guest_addr);
		func_dc.put(guest_addr, NULL);
		func->eraseFromParent();
	}

	if ((vsb = getCachedVSB(guest_addr)) != NULL) {
		vexsb_cache.erase(guest_addr);
		vexsb_dc.put(guest_addr, NULL);
		delete vsb;
	}
}

void VexFCache::dumpLog(std::ostream& os) const
{
	xlate->dumpLog(os);
}

void VexFCache::flush(void)
{
	foreach (it, vexsb_cache.begin(), vexsb_cache.end()) {
		VexSB*	vsb = it->second;
		delete vsb;
	}

	vexsb_cache.clear();
	vexsb_dc.flush();

	foreach (it, func_cache.begin(), func_cache.end()) {
		it->second->eraseFromParent();
	}
	func_cache.clear();
	func_dc.flush();
}

void VexFCache::flush(guest_ptr begin, guest_ptr end)
{
	std::list<guest_ptr>	delete_addrs;

	/* delete VSBs that fall in range */
	foreach (it,
		vexsb_cache.lower_bound(begin), vexsb_cache.upper_bound(end))
	{
		VexSB*	vsb = it->second;
		delete_addrs.push_back(it->first);
		delete vsb;
	}

	foreach (it, delete_addrs.begin(), delete_addrs.end()) {
		vexsb_cache.erase(*it);
		vexsb_dc.put(*it, NULL);
	}
	delete_addrs.clear();

	/* delete funcs that fall in range */
	foreach (it,
		func_cache.lower_bound(begin), func_cache.upper_bound(end))
	{
		delete_addrs.push_back(it->first);
		it->second->eraseFromParent();
	}

	foreach (it, delete_addrs.begin(), delete_addrs.end()) {
		func_cache.erase(*it);
		func_dc.put(*it, NULL);
	}
}
