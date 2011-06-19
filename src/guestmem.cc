#include <assert.h>
#include <iostream>
#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>

#include "Sugar.h"
#include "guestmem.h"

/* XXX other archs? */
#define PAGE_SIZE 4096

GuestMem::GuestMem(void)
: top_brick(NULL)
{
}

GuestMem::~GuestMem(void)
{
}

void* GuestMem::brk()
{
	return top_brick;
}

bool GuestMem::sbrk(void* new_top)
{
	GuestMem::Mapping	m;
	bool			found;
	size_t			add_len;
	void			*old_brk;
	void			*addr;

	old_brk = top_brick;
	if (old_brk == NULL) {
		/* setup by loader phase */
		top_brick = new_top;
		return true;
	}

	found = lookupMapping((char*)old_brk - 1, m);
	if (!found)
		return false;

	add_len = (char*)new_top - (char*)m.end();
	add_len = (add_len + PAGE_SIZE - 1) & ~(PAGE_SIZE -1);

	/* update top so it's page aligned */
	new_top = (char*)m.end() + add_len;

	/* map in missing data */
	addr = mmap(
		m.end(), add_len,
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANON | MAP_FIXED,
		-1, 0);

	if (addr == MAP_FAILED && errno != EFAULT) return false;
	assert (addr != MAP_FAILED && "sbrk is broken again");

	m.length += add_len;
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
