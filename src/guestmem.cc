#include <assert.h>
#include <iostream>
#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdlib.h>

#include "Sugar.h"
#include "guestmem.h"

/* XXX other archs? */
#define PAGE_SIZE 4096
#define BRK_RESERVE 256 * 1024 * 1024

GuestMem::GuestMem(void)
: top_brick(0)
, base_brick(0)
, is_32_bit(false)
, reserve_brick(0)
, base(NULL)
, force_flat(!getenv("VEXLLVM_4GB_REBASE"))
{

#ifdef __amd64__
	/* an exact identity mapping causes problems running
	   android binaries which have been prelinked.  the 
	   prelinking phase put many of the libraries where
	   the kernel module load region is 0xa0000000.  so
	   for 64-bit host, we can just offset everything by
	   4 GB and do the memory mappings ourselves.  this
	   is kind of a kludge but it is probably better than
	   doing a page table based implementation.  obviously,
	   the prelinking will cause trouble running vexllvm
	   natively on android anyway (the libs would definitely)
	   overlap, so let's defer that for another day... */
	if(!force_flat)
		base = (char*)(uintptr_t)0x100000000;
#endif
	/* since we are managing the address space now, 
	   record the null page so we don't reuse it */
	Mapping null_page(guest_ptr(0), PAGE_SIZE, 0);
	recordMapping(null_page);
}

GuestMem::~GuestMem(void)
{
}

guest_ptr GuestMem::brk()
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
	bool found = findRegion(BRK_RESERVE, m);
	assert(found && "couldn't make mapping");
	addr = ::mmap(getBase() + m.offset.o, BRK_RESERVE, prot, 
		flags, -1, 0);
	/* hmm, we could shrink the reserve but */
	assert(addr != MAP_FAILED && "initial sbrk failed");
	m.offset = guest_ptr((char*)addr - getBase());
	assert(!is_32_bit || m.offset.o < 0xFFFFFFFFULL);
	addr = ::mremap(addr, BRK_RESERVE, PAGE_SIZE, 0);
	assert(addr != MAP_FAILED && "sbrk shrink assploded");
	flags &= ~MAP_NORESERVE;
	flags |= MAP_FIXED;

	addr = ::mmap(addr, PAGE_SIZE, prot, flags, -1, 0);
	assert(addr != MAP_FAILED && "sbrk flags fix kerplunked");

	m.length = PAGE_SIZE;
	m.cur_prot = m.req_prot = prot;
	recordMapping(m);
	
	top_brick = base_brick = m.offset;
	reserve_brick = base_brick + BRK_RESERVE;
	return true;
}

bool GuestMem::sbrkInitial(guest_ptr new_top) {
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
	addr = ::mmap(getBase() + new_top, PAGE_SIZE, prot, flags, -1, 0);
	assert(addr == getBase() + new_top && "initial forced sbrk failed");
	
	m.offset = new_top;
	m.length = PAGE_SIZE;
	m.cur_prot = m.req_prot = prot;
	recordMapping(m);
		
	top_brick = base_brick = m.offset;
	return true;
}

bool GuestMem::sbrk(guest_ptr new_top)
{
	GuestMem::Mapping	m;
	bool			found;
	size_t			new_len;
	void			*addr;

	if (top_brick == 0) {
		if(new_top == 0)
			return sbrkInitial();
		else
			return sbrkInitial(new_top);
	}

	found = lookupMapping(base_brick, m);
	if (!found)
		return false;

	new_len = new_top - base_brick;
	new_len = (new_len + PAGE_SIZE - 1) & ~(PAGE_SIZE -1);
	
	/* new_top is not page aligned because its really just
	   a number the guest will use... we don't ever use it
	   anymore. */
	
	if(new_len == m.length) {
		top_brick = new_top;
		return true;
	}
		

	/* lame we can be more graceful here and try to do it */ 
	if(reserve_brick && m.offset + new_len > reserve_brick)
		return false;
	/* i want to extend the existing mapping... but it seems like
	   something is not working with that, so do this instead */
	// addr = mremap(m.offset, m.length, new_len, MREMAP_FIXED);
	addr = ::mmap(getBase() + m.end(), new_len - m.length, 
		PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANON,
		-1, 0);
		
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
			lost = i->second.end() - mapping.offset;
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

		lost = mapping.end() - i->second.offset;
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

bool GuestMem::lookupMapping(guest_ptr addr, Mapping& mapping)
{
	const GuestMem::Mapping* m = lookupMappingPtr(addr);
	if(!m)
		return false;
	mapping = *m;
	return true;
}
const GuestMem::Mapping* GuestMem::lookupMappingPtr(guest_ptr addr) {
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
bool GuestMem::findRegion(size_t len, Mapping& m) {
	if(force_flat) {
		int flags = MAP_PRIVATE | MAP_ANON | MAP_NORESERVE;
		if(is_32_bit)
			flags |= MAP_32BIT;
		void* addr = ::mmap(0, len, 0, flags, -1, 0);
		if(addr == MAP_FAILED)
			return false;
		::munmap(addr, len);
		m.offset = guest_ptr((uintptr_t)addr);
		m.length = len;
		return true;
	}
	/* this implementation is perhaps the biggest POS in the world.
	   it should really take a flag to start from the beginning or the
	   end depending on whether we are brk-ing or not.  it should also
	   doing something other than linear search as that is super lame */
	len = (len + PAGE_SIZE - 1) & ~(PAGE_SIZE -1);
	
	//guest_ptr current = guest_ptr(0x100000);
	guest_ptr current = guest_ptr(0x100000000);	
	mapmap_t::iterator i = maps.lower_bound(current);
	if(i != maps.begin())
		--i;
	while(i != maps.end() && i->second.end() < current) {
		++i;
	}
	if(i != maps.end() && i->second.offset <= current) {
		current = i->second.end();
		++i;
	}
	for(; i != maps.end(); ++i) 
	{
		if(i->second.offset - current > len) {
			m.offset = current;
			m.length = len;
			return true;
		}
		current = i->second.end();
	}
	m.offset = current;
	m.length = len;
	assert(!is_32_bit || m.offset.o > 0xFFFFFFFFULL);
	return true;
}
int GuestMem::mmap(guest_ptr& result, guest_ptr addr, 
	size_t len, int prot, int flags, int fd, off_t offset)
{
	// std::cerr << "mmap("
	// 	<< (void*)addr.o << ", "
	// 	<< (void*)len << ", "
	// 	<< (void*)prot << ", "
	// 	<< (void*)flags << ", "
	// 	<< (void*)fd << ", "
	// 	<< (void*)offset
	// 	<< ")" << std::endl;

	if(fd >= 0 && (offset & (PAGE_SIZE - 1)))
		return -EINVAL;
	if(fd >= 0 && (addr.o & (PAGE_SIZE - 1)))
		return EINVAL;

	if((addr & (PAGE_SIZE - 1)))
		return -EINVAL;

	// std::cerr << "passed checks" << std::endl;

	// /* we are fixing these up as long as a file mapping isn't involved. 
	//    does this match the linux kernel implementation? */
	// len += addr.o & ~(PAGE_SIZE - 1);
	// addr.o &= ~(PAGE_SIZE - 1);
	len = (len + PAGE_SIZE - 1) & ~(PAGE_SIZE -1);
	
	bool fixed = flags & MAP_FIXED;
	Mapping m(addr, len, prot);
	if(addr == 0 && !fixed) {
		if(!findRegion(m.length, m)) {
			// std::cerr << "no region" << std::endl;
			return -ENOMEM;
		}
		fixed = true;
		flags |= MAP_FIXED;
	}
	if (m.req_prot & PROT_WRITE) {
		m.cur_prot &= ~PROT_EXEC;
	}
	void* desired = getBase() + m.offset.o;
	void* at = ::mmap(getBase() + m.offset.o, m.length, 
		m.cur_prot, flags, fd, offset);
	/* this is bad because it will probably be out of our desired
	   address range... so, this is a fail */
	if(at != desired) {
		if(at != MAP_FAILED) {
			::munmap(at, m.length);
		}
		if(fixed) {
			return -errno;
		} 
		/* ok, they didn't force it somewhere, so we can tolerate
		   a modified mapping */
		if(!findRegion(m.length, m)) {
			return -ENOMEM;
		}
		fixed = true;
		flags |= MAP_FIXED;
		at = ::mmap(getBase() + m.offset.o, m.length, 
			m.cur_prot, flags, fd, offset);
	}
	/* fixed is set, so at can only be map failed */
	if(at == MAP_FAILED) {
		return -errno;
	}
	result = m.offset;
	recordMapping(m);
	// std::cerr << "mmap OK => " << (void*)m.offset.o << std::endl;
	return 0;
}
int GuestMem::mprotect(guest_ptr offset, 
	size_t len, int prot)
{
	// std::cerr << "mprotect("
	// 	<< (void*)offset.o << ", "
	// 	<< (void*)len << ", "
	// 	<< (void*)prot
	// 	<< ")" << std::endl;

	if((offset & (PAGE_SIZE - 1)))
		return -EINVAL;
	if((len & (PAGE_SIZE - 1)))
		return -EINVAL;

	/* TODO make sure this is a real mapped region in a better
	   way then if mprotect fails? */
	
	Mapping m(offset, len, prot);
	if (m.req_prot & PROT_WRITE) {
		m.cur_prot &= ~PROT_EXEC;
	}
	int result = ::mprotect(getBase() + offset.o, len, m.cur_prot);
	if(result < 0) {
		return -errno;
	}
	recordMapping(m);
	// std::cerr << "mprotect OK" << std::endl;
	return 0;
}
int GuestMem::munmap(guest_ptr offset, size_t len)
{
	// std::cerr << "munmap("
	// 	<< (void*)offset.o << ", "
	// 	<< (void*)len
	// 	<< ")" << std::endl;
	if((offset & (PAGE_SIZE - 1)))
		return -EINVAL;
	if((len & (PAGE_SIZE - 1)))
		return -EINVAL;

	/* TODO make sure this is a real mapped region in a better
	   way then if munmap fails? */

	Mapping m(offset, len, 0);
	int result = ::munmap(getBase() + offset.o, len);
	if(result < 0) {
		return -errno;
	}
	removeMapping(m);
	// std::cerr << "munmap OK" << std::endl;
	return 0;	
}
int GuestMem::mremap(guest_ptr& result, guest_ptr old_offset, 
	size_t old_length, size_t new_length, int flags, guest_ptr new_offset)
{
	// std::cerr << "mremap("
	// 	<< (void*)old_offset.o << ", "
	// 	<< (void*)old_length << ", "
	// 	<< (void*)new_length << ", "
	// 	<< (void*)flags << ", "
	// 	<< (void*)new_offset.o 
	// 	<< ")" << std::endl;
		
	if((old_offset & (PAGE_SIZE - 1)))
		return -EINVAL;
	if((old_length & (PAGE_SIZE - 1)))
		return -EINVAL;
	if(flags & MREMAP_FIXED && (new_offset & (PAGE_SIZE - 1)))
		return -EINVAL;
	if((new_length & (PAGE_SIZE - 1)))
		return -EINVAL;
	bool fixed = flags & MREMAP_FIXED;
	bool maymove = flags & MREMAP_MAYMOVE;
	
	if(!maymove && fixed && old_offset != new_offset) {
		return -EINVAL;
	}

	Mapping m;
	bool found = lookupMapping(old_offset, m);
	if(!found)
		return -EINVAL;
	
	if(old_offset != m.offset)
		return -EINVAL;
	if(old_length != m.length)
		return -EINVAL;

	Mapping n = m;
	if(!fixed && !maymove) {
		mapmap_t::iterator i = 
			maps.lower_bound(m.offset + new_length);
		if(i != maps.end() && 
			i->second.offset < m.offset + new_length) 
		{
			return -ENOMEM;
		}
		n.length = new_length;
	} else if(fixed) {
		n.offset = new_offset;
		n.length = new_length;
	} else if(maymove) {
		mapmap_t::iterator i = 
			maps.lower_bound(m.offset + new_length);
		if(i != maps.end() && 
			i->second.offset < m.offset + new_length) 
		{
			if(!findRegion(new_length, n)) {
				return -ENOMEM;
			}
		}
		fixed = true;
		flags |= MREMAP_FIXED;
	}
	
	void* desired = getBase() + n.offset.o;
	void* at = ::mremap(getBase() + m.offset.o, m.length, n.length,
		flags, n.offset.o);
	/* if something was allowed to move, then it would have become
	   fixed, so this can only be map failed or the correct address */
	if(at != desired) {
		assert(at == MAP_FAILED);
		return -errno;
	}
	result = n.offset;
	removeMapping(m);
	recordMapping(n);
	// std::cerr << "mremap OK => " << (void*)n.offset.o << std::endl;
	return 0;
}

