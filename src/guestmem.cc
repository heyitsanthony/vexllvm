#include <assert.h>
#include <iostream>
#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include "Sugar.h"
#include "guestmem.h"
#include "guestptimg.h"	//dumpSelfMap

/* XXX other archs? */
#define PAGE_SIZE 4096
#define BRK_RESERVE 256 * 1024 * 1024

GuestMem::GuestMem(void)
: base(NULL)
, top_brick(0)
, base_brick(0)
, reserve_brick(0)
, is_32_bit(false)
, force_flat(getenv("VEXLLVM_4GB_REBASE") == NULL)
, syspage_data(NULL)
{
	const char	*base_str;
#ifdef __amd64__
	/* this is probably only useful for 32-bit binaries
	   or binaries that don't need the evil kernel page
	   @ f*******xxxxx... */
	if (!force_flat)
		base = (char*)(uintptr_t)0x100000000;
#endif
	base_str = getenv("VEXLLVM_BASE_BIAS");
	if (base_str) {
		assert (base == NULL && "Double setting base!");
		if (base_str[1] == 'x') {
			size_t	scan_nr;
			scan_nr = sscanf(base_str+2, "%lx", (long*)&base);
			assert (scan_nr == 1);
		} else {
			base = (char*)((uintptr_t)atoi(base_str));
		}
		fprintf(stderr, "[VEXLLVM] Rebased to %p\n", base);
	}

	/* since we are managing the address space now,
	   record the null page so we don't reuse it */
	Mapping null_page(guest_ptr(0), PAGE_SIZE, PROT_NONE);
	recordMapping(null_page);
}

GuestMem::~GuestMem(void)
{
	foreach (it, maps.begin(), maps.end()) {
		Mapping	*m = it->second;
		::munmap(getHostPtr(m->offset), m->length);
		delete m;
	}

	if (syspage_data) delete [] syspage_data;
}

/* XXX I haven't audited this, so it's very likely that it's wrong. -AJR */
bool GuestMem::sbrkInitial()
{
	GuestMem::Mapping m;
	void *addr;
	int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
	int prot = PROT_READ | PROT_WRITE;

	/* we pick it */
	#ifdef __amd64__
		if(is_32_bit)
			flags |= MAP_32BIT;
	#endif
	/* this sux, for some reason it keeps redirecting the allocation
	   lower, but the truth is, we're forcing it elsewhere anyway, so
	   it doesn't matter.  it would be good to reserve the whole address
	   space for a 32-bit proc */
	if (!force_flat) flags |= MAP_FIXED;

	bool found = findFreeRegion(BRK_RESERVE, m);
	assert(found && "couldn't allocate mapping");

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

bool GuestMem::sbrkInitial(guest_ptr new_top)
{
	GuestMem::Mapping m;
	void *addr;
	int flags = MAP_PRIVATE | MAP_ANONYMOUS;
	int prot = PROT_READ | PROT_WRITE;

	/* they picked it (e.g. xchk), try to make a mapping there.
	   note that this initial mapping doesn't reserve any extra
	   space, so jerks could map over it and screw us.  in this
	   case you'd eventually see divergence in xchk, manifested
	   by -ENOMEM being returned for the vex process on brk when
	   the real process managed to extend the brk */
	addr = ::mmap(getHostPtr(new_top), PAGE_SIZE, prot, flags, -1, 0);
	assert(addr == getHostPtr(new_top) && "initial forced sbrk failed");

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
	size_t			new_len;
	void			*addr;

	if (top_brick == 0)
		return (new_top == 0)
			? sbrkInitial()
			: sbrkInitial(new_top);


	if (!lookupMapping(base_brick, m))
		return false;

	new_len = new_top - base_brick;
	new_len = (new_len + PAGE_SIZE - 1) & ~(PAGE_SIZE -1);

	/* new_top is not page aligned because its really just
	   a number the guest will use... we don't ever use it
	   anymore. */
	if (new_len == m.length) {
		top_brick = new_top;
		return true;
	}

	/* lame we can be more graceful here and try to do it */
	if(reserve_brick && m.offset + new_len > reserve_brick)
		return false;

	/* i want to extend the existing mapping... but it seems like
	   something is not working with that, so do this instead */
	// addr = mremap(m.offset, m.length, new_len, MREMAP_FIXED);
	addr = ::mmap(
		getBase() + m.end(),
		new_len - m.length,
		PROT_READ | PROT_WRITE,
		MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS,
		-1,
		0);

	if (addr == MAP_FAILED && errno != EFAULT) return false;
	assert (addr != MAP_FAILED && "sbrk is broken again");

	m.length = new_len;
	recordMapping(m);
	top_brick = new_top;
	return true;
}

void GuestMem::recordMapping(Mapping& mapping)
{
	Mapping	*cur_m = NULL;

	assert(((uintptr_t)mapping.offset & (PAGE_SIZE - 1)) == 0 &&
		"Mapping offset not page-aligned");

	mapping.length += (PAGE_SIZE - 1);
	mapping.length &= ~(PAGE_SIZE - 1);

	mapmap_t::iterator it = maps.lower_bound(mapping.offset);
	if (it != maps.begin()) --it;

	cur_m = (it != maps.end()) ? it->second : NULL;

	if (cur_m && cur_m->offset < mapping.offset) {
		/* we are cutting off someone before us */
		if (cur_m->end() > mapping.offset) {
			long lost;
			/* we are completely inside the old block */
			if (mapping.end() < cur_m->end()) {
				Mapping *smaller;

				smaller = new Mapping(
					mapping.end(),
					(long)cur_m->end()-(long)mapping.end(),
					cur_m->req_prot);

				smaller->cur_prot = cur_m->cur_prot;
				maps.insert(
					std::make_pair(
						smaller->offset,
						smaller));
			}
			lost = cur_m->end() - mapping.offset;
			cur_m->length -= lost;
		}
		++it;
	}

	cur_m = (it != maps.end()) ? it->second : NULL;

	/* mapping overlaps completly */
	if (cur_m && cur_m->offset == mapping.offset) {
		if (cur_m->length  == mapping.length) {
			it->second = new Mapping(mapping);
			delete cur_m;
			return;
		}

		/* old mapping is bigger, split */
		if(cur_m->length > mapping.length) {
			Mapping	*smaller;

			smaller = new Mapping(
				mapping.end(),
				cur_m->length - mapping.length,
				cur_m->req_prot);
			smaller->cur_prot = cur_m->cur_prot;

			maps.insert(std::make_pair(smaller->offset, smaller));

			it->second = new Mapping(mapping);
			delete cur_m;
			return;
		}

		/* newer mapping is bigger, replace it */
		it->second = new Mapping(mapping);
		delete cur_m;
		++it;
	} else {
		/* no specific one to overwrite, just add it */
		it = maps.insert(std::make_pair(
			mapping.offset,
			new Mapping(mapping))).first;
		++it;
	}

	/* kill all the ones it overlaps */
	while (it != maps.end() && mapping.end() > it->second->end()) {
		mapmap_t::iterator to_erase = it++;

		delete to_erase->second;
		maps.erase(to_erase);
	}

	/* trim the last one if necessary */
	cur_m = (it != maps.end()) ? it->second : NULL;
	if (cur_m && mapping.end() > cur_m->offset) {
		long lost;

		lost = mapping.end() - cur_m->offset;
		cur_m->offset = mapping.end();
		cur_m->length -= lost;
	}
}

void GuestMem::removeMapping(Mapping& mapping)
{
	mapmap_t::iterator	it;

	/* this reuses the collision handling code in the insert
	   function.  could be more efficient, but meh */
	recordMapping(mapping);

	it = maps.find(mapping.offset);
	if (it != maps.end()) {
		delete it->second;
		maps.erase(it);
	}
}

bool GuestMem::lookupMapping(guest_ptr addr, Mapping& mapping)
{
	const GuestMem::Mapping* m;

	if ((m = lookupMapping(addr)) == NULL)
		return false;

	mapping = *m;
	return true;
}

/* return mapping that begins at addr */
const GuestMem::Mapping* GuestMem::lookupMapping(guest_ptr addr) const
{
	mapmap_t::const_iterator it;
	it = maps.lower_bound(addr);

	if (it != maps.end() && it->first == addr)
		return it->second;

	if (it == maps.begin())
		return NULL;

	--it;
	if (it->second->end() > addr)
		return it->second;

	return NULL;
}

/* find mapping (if any) that contains the given address */
const GuestMem::Mapping* GuestMem::findOwner(guest_ptr addr) const
{
	const Mapping		*m;
	mapmap_t::const_iterator it;

	it = maps.find(addr);
	if (it != maps.end())
		return it->second;

	/* first mapping  >addr */
	it = maps.upper_bound(addr);
	if (it == maps.begin())
		return NULL;

	/* last mapping <= addr */
	it--;
	m = it->second;
	if (m->contains(addr))
		return m;

	return NULL;
}

/* return mapping that follows directly after 'addr' (but does not contain it) */
const GuestMem::Mapping* GuestMem::findNextMapping(guest_ptr addr) const
{
	mapmap_t::const_iterator it;
	const Mapping		*m;

	it = maps.lower_bound(addr);
	if (it == maps.end() || it == maps.end())
		return NULL;

	m = it->second;
	if (m->contains(addr))
		it++;

	if (it == maps.end()) return NULL;

	return it->second;
}

std::list<GuestMem::Mapping> GuestMem::getMaps(void) const
{
	std::list<Mapping>	ret;

	foreach (it, maps.begin(), maps.end()) {
		Mapping	m(*(it->second));
		if (!(m.req_prot & PROT_READ))
			continue;
		ret.push_back(m);
	}

	return ret;
}

void GuestMem::Mapping::print(std::ostream& os) const
{
	os	<< "Addr: " << (void*)offset.o << "--" << (void*)end().o
		<< ". ReqProt=" << std::hex << req_prot
		<< ". CurProt=" << cur_prot
		<< std::endl;
}

/* TODO: some kind of check to see if we blew out address space
   thats ok in 64-bit, or possibly using the host mapping capability
   to just find something higher than the rebase */
bool GuestMem::findFreeRegion(size_t len, Mapping& m) const
{
	if (force_flat) {
		void	*addr;
		int	flags;

		flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
		if (is_32_bit) flags |= MAP_32BIT;

		addr = ::mmap(0, len, 0, flags, -1, 0);
		if (addr == MAP_FAILED)
			return false;
		::munmap(addr, len);

		m.offset = guest_ptr((uintptr_t)addr - (uintptr_t)getBase());
		m.length = len;
		return true;
	}

	return findFreeRegionByMaps(len, m);
}

bool GuestMem::findFreeRegionByMaps(size_t len, Mapping& m) const
{
	guest_ptr			current(0x100000);
	mapmap_t::const_iterator	it;

	len = (len + PAGE_SIZE - 1) & ~(PAGE_SIZE -1);

	if (current < reserve_brick)
		current = reserve_brick;

	it = maps.lower_bound(current);

	if (it != maps.begin()) --it;
	while (it != maps.end() && it->second->end() < current)
		++it;

	if (it != maps.end() && it->second->offset <= current) {
		current = it->second->end();
		++it;
	}

	for (; it != maps.end(); ++it) {
		if (it->second->offset - current > len) {
			m.offset = current;
			m.length = len;
			return true;
		}

		current = it->second->end();
	}

	m.offset = current;
	m.length = len;
	assert (!is_32_bit || m.offset.o < 0xFFFFFFFFULL);
	return true;
}

void GuestMem::print(std::ostream& os) const
{
	os << "Guest Mem (" << maps.size() << ") entries: \n";
	foreach (it, maps.begin(), maps.end()) {
		it->second->print(os);
	}
	os << "\n";
}

int GuestMem::mmap(guest_ptr& result, guest_ptr addr,
	size_t len, int prot, int flags, int fd, off_t offset)
{
	void	*desired, *at;

	result = guest_ptr(0);

	if (fd >= 0 && (offset & (PAGE_SIZE - 1))) return -EINVAL;
	if (fd >= 0 && (addr.o & (PAGE_SIZE - 1))) return -EINVAL;
	if ((addr & (PAGE_SIZE - 1))) return -EINVAL;

	// /* we are fixing these up as long as a file mapping isn't involved.
	//    does this match the linux kernel implementation? */
	// len += addr.o & ~(PAGE_SIZE - 1);
	// addr.o &= ~(PAGE_SIZE - 1);
	len = (len + PAGE_SIZE - 1) & ~(PAGE_SIZE -1);

	Mapping m(addr, len, prot);
	if (addr == 0 && !(flags & MAP_FIXED)) {
		if (!findFreeRegion(m.length, m))
			return -ENOMEM;
	}

	if (m.req_prot & PROT_WRITE) m.cur_prot &= ~PROT_EXEC;

	desired = getHostPtr(m.offset);

	at = ::mmap(desired, m.length, m.cur_prot, flags & ~MAP_FIXED, fd, offset);
	if (at == desired) goto success;

	/* tear down incorrect allocation, if given */
	if (at != MAP_FAILED) ::munmap(at, m.length);

	/* Memory at desired location is not available through suggestion.
	 * Presumably, this means it is already allocated. */
	if ((flags & MAP_FIXED) == 0) {
		/* If it's not a fixed mapping, we can try to relocate it */
		if (!findFreeRegion(m.length, m))
			return -ENOMEM;
	} else {
		assert (flags & MAP_FIXED);
		if (!canUseRange(addr, len))
			return -ENOMEM;
	}

	desired = getHostPtr(m.offset);
	at = ::mmap(desired, m.length, m.cur_prot, flags, fd, offset);
	if (at == MAP_FAILED) return -errno;
	if (at != desired) {
		::munmap(at, m.length);
		return -errno;
	}

success:
	result = m.offset;
	recordMapping(m);
	return 0;
}

bool GuestMem::canUseRange(guest_ptr base, unsigned int len) const
{
	guest_ptr	cur_ptr(base);
	guest_ptr	end_ptr(base + len);

	while (cur_ptr < end_ptr) {
		const Mapping	*m;
		unsigned int	test_len;
		void		*test_addr;

		/* mapping exists in guest? */
		m = findOwner(cur_ptr);
		if (m != NULL) {
			assert (cur_ptr.o >= m->offset.o);
			cur_ptr.o += m->length - (cur_ptr.o - m->offset.o);
			continue;
		}

		/* check to see if mapping isn't present in host */
		m = findNextMapping(cur_ptr);
		test_len = end_ptr - cur_ptr;
		if (m != NULL) {
			unsigned int	gap;
			assert (m->offset > cur_ptr);
			gap = m->offset - cur_ptr;
			if (gap < test_len) test_len = gap;
		}

		test_addr = ::mmap(
			getHostPtr(base),
			test_len,
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS,
			-1,
			0);
		if (test_addr != MAP_FAILED) ::munmap(test_addr, test_len);
		if (test_addr != getHostPtr(base))
			return false;

		cur_ptr.o += test_len;
	}

	return true;
}

int GuestMem::mprotect(guest_ptr p, size_t len, int prot)
{
	int	err;

	if ((p & (PAGE_SIZE - 1))) return -EINVAL;
	if ((len & (PAGE_SIZE - 1))) return -EINVAL;

	/* TODO make sure this is a real mapped region in a better
	   way then if mprotect fails? */

	Mapping m(p, len, prot);
	if (m.req_prot & PROT_WRITE)
		m.cur_prot &= ~PROT_EXEC;

	err = ::mprotect(getHostPtr(p), len, m.cur_prot);
	if (err < 0) return -errno;

	recordMapping(m);
	return 0;
}

int GuestMem::munmap(guest_ptr addr, size_t len)
{
	Mapping m(addr, len, 0);
	Mapping cur_mapping;
	int	err;

	if ((addr & (PAGE_SIZE - 1))) return -EINVAL;

	// old behavior was to return an -EINVAL on a
	// non-multiple of PAGE_SIZE, correct behavior is given by man page:
	// "All pages containing a part of the indicated range are unmapped"
	len = (len + PAGE_SIZE - 1) & ~((size_t)PAGE_SIZE - 1);

	/* TODO make sure this is a real mapped region in a better
	   way then if munmap fails? */
	if (!lookupMapping(addr, cur_mapping)) {
		std::cerr << "[VEXLLVM] trying to munmap missing mapping: ";
		m.print(std::cerr);
		std::cerr << '\n';
	}

	err = ::munmap(getHostPtr(addr), len);
	if (err < 0)
		return -errno;

	removeMapping(m);
	return 0;
}

/* XXX I haven't audited this, so it's very likely that it's wrong. -AJR */
int GuestMem::mremap(
	guest_ptr& result, guest_ptr old_offset,
	size_t old_length, size_t new_length, int flags, guest_ptr new_offset)
{
	void	*desired, *at;
	Mapping	m;

	if ((old_offset & (PAGE_SIZE - 1))) return -EINVAL;
	if ((old_length & (PAGE_SIZE - 1))) return -EINVAL;
	if ((new_length & (PAGE_SIZE - 1))) return -EINVAL;

	if (flags & MREMAP_FIXED && (new_offset & (PAGE_SIZE - 1)))
		return -EINVAL;

	bool fixed = flags & MREMAP_FIXED;
	bool maymove = flags & MREMAP_MAYMOVE;

	if (!maymove && fixed && old_offset != new_offset)
		return -EINVAL;

	if (!lookupMapping(old_offset, m))
		return -EINVAL;

	if (old_offset != m.offset) return -EINVAL;
	if (old_length != m.length) return -EINVAL;

	Mapping n = m;
	if(!fixed && !maymove) {
		mapmap_t::iterator it = maps.lower_bound(m.offset + new_length);
		if (	it != maps.end() &&
			it->second->offset < m.offset + new_length)
		{
			return -ENOMEM;
		}
		n.length = new_length;
	} else if (fixed) {
		n.offset = new_offset;
		n.length = new_length;
	} else if (maymove) {
		mapmap_t::iterator i =
			maps.lower_bound(m.offset + new_length);
		if(i != maps.end() &&
			i->second->offset < m.offset + new_length)
		{
			if(!findFreeRegion(new_length, n)) {
				return -ENOMEM;
			}
		}
		fixed = true;
		flags |= MREMAP_FIXED;
	}

	desired = getHostPtr(n.offset);
	at = ::mremap(getHostPtr(m.offset), m.length, n.length, flags, desired);

	/* if something was allowed to move, then it would have become
	   fixed, so this can only be map failed or the correct address */
	if (at != desired) {
		assert(at == MAP_FAILED);
		return -errno;
	}

	result = n.offset;
	removeMapping(m);
	recordMapping(n);
	return 0;
}

void GuestMem::setType(guest_ptr addr, GuestMem::Mapping::MapType mt)
{
	mapmap_t::iterator	it;
	Mapping			*m;

	assert (mt != Mapping::VSYSPAGE);

	it = maps.lower_bound(addr);
	assert (it != maps.end() && "Setting type for unmapped guest addr");

	m = it->second;
	m->type = mt;

	if (mt == Mapping::HEAP) {
		base_brick = m->offset;
		top_brick = m->end();
	}
}

const void* GuestMem::getSysHostAddr(guest_ptr p) const
{
	const Mapping	*owner;

	owner = findOwner(p);
	if (owner == NULL) return NULL;
	if (owner->type != Mapping::VSYSPAGE) return NULL;

	return (const void*)(syspage_data + (p.o - owner->offset.o));
}

void GuestMem::addSysPage(guest_ptr p, char* host_data, unsigned int len)
{
	GuestMem::Mapping	m(p, len, PROT_READ | PROT_EXEC);
	void			*mmap_ret;

	assert (host_data && syspage_data == NULL);

	syspage_data = host_data;
	m.type = Mapping::VSYSPAGE;
	recordMapping(m);

	if (getBase() == NULL)
		return;

	/* try to map it in if relocated */
	mmap_ret = ::mmap(
		getHostPtr(m.offset),
		len,
		PROT_READ | PROT_EXEC | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS,
		-1,
		0);
	assert (mmap_ret != MAP_FAILED);
	memcpy(m.offset, host_data, len);
}

void GuestMem::nameMapping(guest_ptr addr, const std::string& s)
{
	const GuestMem::Mapping* m;

	if (s.size() == 0)
		return;

	if ((m = lookupMapping(addr)) == NULL)
		return;

	std::string	*new_s;

	new_s = new std::string(s);
	mapping_names.push_back(new_s);

	/* fuck it */
	const_cast<GuestMem::Mapping*>(m)->name = new_s;
}
