#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "llvm/Function.h"

#include "Sugar.h"
#include "vexsb.h"
#include "vexxlate.h"
#include "vexfcache.h"

using namespace llvm;

VexFCache::VexFCache(void)
: xlate(new VexXlate()),
  max_cache_ents(~0) /* don't evict by default */
{
	dump_llvm = (getenv("VEXLLVM_DUMP_LLVM")) ? true : false;
}

VexFCache::VexFCache(VexXlate* in_xlate)
: xlate(in_xlate),
  max_cache_ents(~0)
{
	assert (xlate != NULL);
	dump_llvm = (getenv("VEXLLVM_DUMP_LLVM")) ? true : false;
}

VexFCache::~VexFCache(void)
{
	flush();
	delete xlate;
}

void VexFCache::setMaxCache(unsigned int x)
{
	if (x < max_cache_ents) flush();
	max_cache_ents = x;
}

VexSB* VexFCache::getVSB(void* hostptr, uint64_t guest_addr)
{
	VexSB*	vsb;

	vsb = xlate->xlate(hostptr, (uint64_t)guest_addr);
	vexsb_cache[(void*)guest_addr] = vsb;
	vexsb_dc.put((void*)guest_addr, vsb);
	return vsb;
}

VexSB* VexFCache::getCachedVSB(uint64_t guest_addr)
{
	VexSB				*vsb;
	vexsb_map::const_iterator	it;

	vsb = vexsb_dc.get((void*)guest_addr);
	if (vsb) return vsb;

	it = vexsb_cache.find((void*)guest_addr);
	if (it == vexsb_cache.end()) return NULL;

	vsb = (*it).second;
	vexsb_dc.put((void*)guest_addr, vsb);
	return vsb;
}

Function* VexFCache::getCachedFunc(uint64_t guest_addr)
{
	Function			*func;
	func_map::const_iterator	it;

	func = func_dc.get((void*)guest_addr);
	if (func) return func;

	it = func_cache.find((void*)guest_addr);
	if (it == func_cache.end()) return NULL;

	func = (*it).second;
	func_dc.put((void*)guest_addr, func);
	return func;
}

Function* VexFCache::getFunc(void* hostptr, uint64_t guest_addr)
{
	Function*	ret_f;
	VexSB*		vsb;

	ret_f = getCachedFunc(guest_addr);
	if (ret_f) return ret_f;

	vsb = getCachedVSB(guest_addr);
	if (!vsb) {
		vsb = getVSB(hostptr, guest_addr);	
	}

	ret_f = genFunctionByVSB(vsb);
	return ret_f;
}

Function* VexFCache::genFunctionByVSB(VexSB* vsb)
{
	Function			*f;
	char				emitstr[512];
	
	/* not in caches, generate */
	sprintf(emitstr, "sb_%p", (void*)vsb->getGuestAddr());
	f = vsb->emit(emitstr);
	assert (f && "FAILED TO EMIT FUNC??");

	func_cache[(void*)vsb->getGuestAddr()] = f;
	func_dc.put((void*)vsb->getGuestAddr(), f);

	if (dump_llvm) f->dump();

	return f;
}

void VexFCache::evict(uint64_t guest_addr)
{
	Function	*func;
	VexSB		*vsb;

	if ((func = getCachedFunc(guest_addr)) != NULL) {
		func_cache.erase((void*)guest_addr);
		func_dc.put((void*)guest_addr, NULL);
		delete func;
	}

	if ((vsb = getCachedVSB(guest_addr)) != NULL) {
		vexsb_cache.erase((void*)guest_addr);
		vexsb_dc.put((void*)guest_addr, NULL);
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
		delete (*it).second;
	}

	vexsb_cache.clear();
	vexsb_dc.flush();

	func_cache.clear();
	func_dc.flush();
}
