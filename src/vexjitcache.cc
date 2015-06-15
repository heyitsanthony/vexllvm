#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <set>
#include <queue>

#include "Sugar.h"
#include "vexjitcache.h"
#include "jitengine.h"
#include "genllvm.h"

using namespace llvm;

VexJITCache::VexJITCache(std::shared_ptr<VexXlate> xlate,
			 std::unique_ptr<JITEngine> je)
: VexFCache(xlate)
, jit_engine(std::move(je))
{
}

VexJITCache::~VexJITCache(void)
{
	flush();
}

vexfunc_t VexJITCache::getCachedFPtr(guest_ptr guest_addr)
{
	vexfunc_t *fptr;

	fptr = jit_dc.get(guest_addr);
	if (fptr) return (vexfunc_t)fptr;

	auto it = jit_cache.find(guest_addr);
	if (it == jit_cache.end()) return NULL;

	fptr = (vexfunc_t*)(*it).second;
	jit_dc.put(guest_addr, fptr);

	return (vexfunc_t)fptr;
}

/**
 * Since the JIT is really slow, do exit analysis to find extra
 * functions to put in the genLLVM module.
 *
 * Don't generate all the functions (too much extra work).
 * 5.5s => 4.9s
 */
#define FETCH_EXITS	1

#ifdef FETCH_EXITS
static std::set<guest_ptr> get_ret_addrs(Function *f)
{
	std::set<guest_ptr>	ret;

	for (auto& bb : *f) {
	for (auto& ii : bb) {
		auto ri = dyn_cast<ReturnInst>(&ii);
		if (!ri) continue;

		auto ci = dyn_cast<ConstantInt>(ri->getReturnValue());
		if (!ci) continue;

		ret.insert(guest_ptr(ci->getZExtValue()));
	}
	}

	return ret;
}
#endif

vexfunc_t VexJITCache::getFPtr(void* host_addr, guest_ptr guest_addr)
{
	Function	*llvm_f;
	vexfunc_t	ret_f;

	ret_f = getCachedFPtr(guest_addr);
	if (ret_f) return ret_f;

	llvm_f = getFunc(host_addr, guest_addr);
	assert (llvm_f != NULL && "Could not get function");

#ifdef FETCH_EXITS
	std::set<std::pair<guest_ptr, std::string>> fnames;
	std::set<Function*>	complete, workset;
	std::queue<Function*>	s;

	workset.insert(llvm_f);
	s.push(llvm_f);
	if (host_addr == (void*)guest_addr.o)
	while (!workset.empty()) {
		auto f = s.front();

		s.pop();

		auto addrs = get_ret_addrs(f);
		for (auto & addr : addrs) {
			if (jit_cache.count(addr))
				continue;

			auto new_f = getFunc((void*)addr.o, addr);
			if (workset.count(new_f) || complete.count(new_f))
				continue;
			if (!new_f)
				continue;

			fnames.insert(
				std::make_pair(
					addr,
					new_f->getName().str()));
			workset.insert(new_f);
			s.push(new_f);
		}

		workset.erase(f);
		complete.insert(f);

		if (complete.size() + workset.size() > 2)
			break;
	}
#endif

	ret_f = (vexfunc_t)jit_engine->getPointerToFunction(
		llvm_f,
		theGenLLVM->takeModule());
	assert (ret_f != NULL && "Could not JIT");

#ifdef FETCH_EXITS
	for (auto &p : fnames) {
		auto vf = jit_engine->getPointerToNamedFunction(p.second);
		jit_cache[p.first] = (vexfunc_t)vf;
	}
#endif

	jit_cache[guest_addr] = ret_f;
	jit_dc.put(guest_addr, (vexfunc_t*)ret_f);

	return ret_f;
}

void VexJITCache::evict(guest_ptr guest_addr)
{
	Function	*f;
	if ((f = getCachedFunc(guest_addr)) != NULL) {
		delete f;
	}
	jit_cache.erase(guest_addr);
	jit_dc.put(guest_addr, NULL);
	VexFCache::evict(guest_addr);
}

void VexJITCache::flush(void)
{
	jit_cache.clear();
	jit_dc.flush();
	VexFCache::flush();
}

void VexJITCache::flush(guest_ptr begin, guest_ptr end)
{
	foreach (it, funcBegin(begin), funcEnd(end)) {
		jit_cache.erase(it->first);
		jit_dc.put(it->first, NULL);
	}
	VexFCache::flush(begin, end);
}
