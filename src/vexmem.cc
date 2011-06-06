#include "vexmem.h"
#include <assert.h>
#include <iostream>
/* other archs */
#define PAGE_SIZE 4096

VexMem::VexMem(void) {
	
}
VexMem::~VexMem(void) {
	
}
void VexMem::recordMapping(Mapping& mapping) {
	assert(((long)mapping.offset & (PAGE_SIZE - 1)) == 0);
	mapping.length += (PAGE_SIZE - 1);
	mapping.length &= ~(PAGE_SIZE - 1);
	mapmap_t::iterator i = maps.lower_bound(mapping.offset);
	if(i != maps.begin()) 
		--i;
	if(i != maps.end() && i->second.offset < mapping.offset) {
		/* we are cutting off someone before us*/
		if(i->second.end() > mapping.offset) {
			long lost = (char*)i->second.end() - 
				(char*)mapping.offset;
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
			Mapping smaller;
			smaller.offset = mapping.end();
			smaller.length = i->second.length - mapping.length;
			smaller.req_prot = i->second.req_prot;
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
		i = maps.insert(
			std::make_pair(mapping.offset, mapping)).first;
		++i;
	}
	
	/* now kill all the ones it overlaps */
	while(i != maps.end() && mapping.end() > i->second.end()) {
		mapmap_t::iterator to_erase = i++;
		maps.erase(to_erase);
	}
	/* now trim the last one if necessary */
	if(i != maps.end() && mapping.end() > i->second.offset) {
		long lost = (char*)mapping.end() - 
			(char*)i->second.offset;
		i->second.offset = mapping.end();
		i->second.length -= lost;
	}
}
void VexMem::removeMapping(Mapping& mapping) {
	/* this reuses the collision handling code in the insert
	   function.  could be more efficient, but meh */
	recordMapping(mapping);
	maps.erase(mapping.offset);
}
bool VexMem::lookupMapping(void* addr, Mapping& mapping) {
	mapmap_t::iterator i = maps.lower_bound(addr);
	if(i != maps.end() && i->first == addr) {
		mapping = i->second;
		return true;
	} 
	if(i == maps.begin()) 
		return false;
	--i;
	if(i->second.end() > addr) {
		mapping = i->second;
		return true;
	}
	return false;
}
