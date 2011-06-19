#include <assert.h>
#include <iostream>
#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>

#include "Sugar.h"
#include "guestmem.h"

/* XXX other archs? */
#define PAGE_SIZE 4096
#define BRK_RESERVE 256 * 1024 * 1024

GuestMem::GuestMem(void)
: top_brick(NULL)
, base_brick(NULL)
, is_32_bit(false)
, reserve_brick(NULL)
{
}

GuestMem::~GuestMem(void)
{
}

void* GuestMem::brk()
{
	return top_brick;
}
bool GuestMem::sbrkInitial() {
	GuestMem::Mapping m;
	void *addr;
	int flags = MAP_PRIVATE | MAP_ANON | MAP_NORESERVE;
	int prot = PROT_READ | PROT_WRITE;

	/* we pick it */
	#ifdef __amd64__
		if(is_32_bit)
			flags |= MAP_32BIT;
	#endif
	addr = mmap(NULL, BRK_RESERVE, prot, flags, -1, 0);
	/* hmm, we could shrink the reserve but */
	assert(addr != MAP_FAILED && "initial sbrk failed");
	addr = mremap(addr, BRK_RESERVE, PAGE_SIZE, 0);
	assert(addr != MAP_FAILED && "sbrk shrink assploded");
	flags &= ~MAP_NORESERVE;
	flags |= MAP_FIXED;
	addr = mmap(addr, PAGE_SIZE, prot, flags, -1, 0);
	assert(addr != MAP_FAILED && "sbrk flags fix kerplunked");

	m.offset = addr;
	m.length = PAGE_SIZE;
	m.cur_prot = m.req_prot = prot;
	recordMapping(m);
	
	top_brick = base_brick = m.offset;
	reserve_brick = (char*)base_brick + BRK_RESERVE;
	return true;
}

bool GuestMem::sbrkInitial(void* new_top) {
	GuestMem::Mapping m;
	void *addr;
	int flags = MAP_PRIVATE | MAP_ANON;
	int prot = PROT_READ | PROT_WRITE;

	/* they picked it (e.g. xchk), try to make a mapping there. 
	   note that this initial mapping doesn't reserve any extra
	   space, so jerks could map over it and screw us.  in this
	   case you'd eventually see divergence in xchk, manifested
	   by -ENOMEM being returned for the vex process on brk when
	   the real process managed to extend the brk */
	addr = mmap(new_top, PAGE_SIZE, prot, flags, -1, 0);
	assert(addr == new_top && "initial forced sbrk failed");
	
	m.offset = addr;
	m.length = PAGE_SIZE;
	m.cur_prot = m.req_prot = prot;
	recordMapping(m);
		
	top_brick = base_brick = m.offset;
	return true;
}

bool GuestMem::sbrk(void* new_top)
{
	GuestMem::Mapping	m;
	bool			found;
	size_t			new_len;
	void			*addr;

	if (top_brick == NULL) {
		if(new_top == NULL)
			return sbrkInitial();
		else
			return sbrkInitial(new_top);
	}

	found = lookupMapping((char*)base_brick, m);
	if (!found)
		return false;

	new_len = (char*)new_top - (char*)base_brick;
	new_len = (new_len + PAGE_SIZE - 1) & ~(PAGE_SIZE -1);
	
	/* new_top is not page aligned because its really just
	   a number the guest will use... we don't ever use it
	   anymore. */
	
	if(new_len == m.length) {
		top_brick = new_top;
		return true;
	}
		

	/* lame we can be more graceful here and try to do it */ 
	if(reserve_brick && (char*)m.offset + new_len > reserve_brick)
		return false;

	/* i want to extend the existing mapping... but it seems like
	   something is not working with that, so do this instead */
	// addr = mremap(m.offset, m.length, new_len, MREMAP_FIXED);
	addr = mmap(m.end(), new_len - m.length, PROT_READ | PROT_WRITE,
		MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0);
		
	if (addr == MAP_FAILED && errno != EFAULT) return false;
	assert (addr != MAP_FAILED && "sbrk is broken again");

	m.length = new_len;
	recordMapping(m);
	top_brick = new_top;
	return true;
}

void GuestMem::recordMapping(Mapping& mapping)
{
	assert(((uintptr_t)mapping.offset & (PAGE_SIZE - 1)) == 0 &&
		"Mapping offset not page-aligned");

	mapping.length += (PAGE_SIZE - 1);
	mapping.length &= ~(PAGE_SIZE - 1);

	mapmap_t::iterator i = maps.lower_bound(mapping.offset);
	if (i != maps.begin()) --i;

	if (i != maps.end() && i->second.offset < mapping.offset) {
		/* we are cutting off someone before us */
		if(i->second.end() > mapping.offset) {
			long lost;
			/* we are completely inside the old block */
			if(mapping.end() < i->second.end()) {
				Mapping smaller(
					mapping.end(), (long)i->second.end()-
						(long)mapping.end(),
					i->second.req_prot);
				smaller.cur_prot = i->second.cur_prot;
				maps.insert(std::make_pair(
					smaller.offset, smaller));
			}
			lost = (char*)i->second.end() - (char*)mapping.offset;
			i->second.length -= lost;
		}
		++i;
	}

	/* mapping overlaps completly */
	if(i != maps.end() && i->first == mapping.offset) {
		if(i->second.length == mapping.length) {
			i->second = mapping;
			return;
		}
		/* old mapping is bigger, split */
		if(i->second.length > mapping.length) {
			Mapping smaller(
				mapping.end(),
				i->second.length - mapping.length,
				i->second.req_prot);
			smaller.cur_prot = i->second.cur_prot;
			maps.insert(std::make_pair(smaller.offset, smaller));
			i->second = mapping;
			return;
		}
		/* newer mapping is bigger, replace it */
		i->second = mapping;
		++i;
	} else {
		/* no specific one to overwrite, just add it */
		i = maps.insert(std::make_pair(mapping.offset, mapping)).first;
		++i;
	}

	/* now kill all the ones it overlaps */
	while(i != maps.end() && mapping.end() > i->second.end()) {
		mapmap_t::iterator to_erase = i++;
		maps.erase(to_erase);
	}

	/* now trim the last one if necessary */
	if(i != maps.end() && mapping.end() > i->second.offset) {
		long lost;

		lost = (char*)mapping.end() - (char*)i->second.offset;
		i->second.offset = mapping.end();
		i->second.length -= lost;
	}
}

void GuestMem::removeMapping(Mapping& mapping)
{
	/* this reuses the collision handling code in the insert
	   function.  could be more efficient, but meh */
	recordMapping(mapping);
	maps.erase(mapping.offset);
}

bool GuestMem::lookupMapping(void* addr, Mapping& mapping)
{
	const GuestMem::Mapping* m = lookupMappingPtr(addr);
	if(!m)
		return false;
	mapping = *m;
	return true;
}
const GuestMem::Mapping* GuestMem::lookupMappingPtr(void* addr) {
	mapmap_t::iterator i = maps.lower_bound(addr);
	if(i != maps.end() && i->first == addr) {
		return &i->second;
	}
	if(i == maps.begin())
		return NULL;
	--i;
	if(i->second.end() > addr) {
		return &i->second;
	}
	return NULL;
}

std::list<GuestMem::Mapping> GuestMem::getMaps(void) const
{
	std::list<Mapping>	ret;

	foreach (it, maps.begin(), maps.end()) {
		Mapping	m(it->second);
		if (!(m.req_prot & PROT_READ))
			continue;
		ret.push_back(m);
	}

	return ret;
}

void GuestMem::Mapping::print(std::ostream& os) const
{
	os	<< "Addr: " << offset << "--" << end() << ". ReqProt="
		<< std::hex << req_prot << ". CurProt=" << cur_prot
		<< std::endl;
}
