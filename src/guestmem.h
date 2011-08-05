#ifndef GUESTMEM_H
#define GUESTMEM_H

#include <iostream>
#include <list>
#include <map>
#include <string.h>
#include "guestptr.h"

class GuestMem
{
public:

/* extent of guest memory range; may need to extend later so 
 * getData works on non-identity mappings */
class Mapping {
public:
	Mapping(void) : offset(0), length(0) {}
	Mapping(guest_ptr in_off, size_t in_len, int prot, 
		bool in_is_stack=false)
	: offset(in_off)
	, length(in_len)
	, req_prot(prot)
	, cur_prot(prot)
	, is_stack(in_is_stack)
	, unmapped(false){}
	virtual ~Mapping(void) {}

	guest_ptr end() const { return offset + length; }
	
	unsigned int getBytes(void) const { return length; }
	bool isStack(void) const { return is_stack; }
	int getReqProt(void) const { return req_prot; }
	int getCurProt(void) const { return cur_prot; }
	void print(std::ostream& os) const;

	bool isValid() const { return offset && length; }
	void markUnmapped() { unmapped = true; }
	bool wasUnmapped() { return unmapped; }

	guest_ptr	offset;
	size_t		length;
	int		req_prot;
	int		cur_prot;
	bool		is_stack;
	bool		unmapped;
};

	GuestMem(void);
	virtual ~GuestMem(void);
	void recordMapping(Mapping& mapping);
	const Mapping* lookupMappingPtr(guest_ptr addr);
	void removeMapping(Mapping& mapping);
	bool lookupMapping(guest_ptr addr, Mapping& mapping);
	guest_ptr brk();
	bool sbrk(guest_ptr new_top);
	std::list<Mapping> getMaps(void) const;
	void mark32Bit() { is_32_bit = true; }
	bool is32Bit() const { return is_32_bit; }
	char* getBase() const { return base; }
	bool findRegion(size_t len, Mapping& m);
	bool isFlat() const { return force_flat; }
	
	void* getData(const Mapping& m) const { return (void*)(base + m.offset.o); }

	/* access small bits of guest memory */
	template <typename T>
	T read(guest_ptr offset) {
		return *(T*)(base + offset.o);
	}
	uintptr_t readNative(guest_ptr offset, int idx = 0) {
		if(is_32_bit)
			return *(unsigned int*)(base + offset.o + idx*4);
		else
			return *(unsigned long*)(base + offset.o + idx*8);
	}
	template <typename T>
	void write(guest_ptr offset, const T& t) {
		*(T*)(base + offset.o) = t;
	}
	void writeNative(guest_ptr offset, uintptr_t t) {
		if(is_32_bit)
			*(unsigned int*)(base + offset.o) = t;
		else
			*(unsigned long*)(base + offset.o) = t;
	}

	/* access large blocks of guest memory */
	void memcpy(guest_ptr dest, const void* src, size_t len) {
		::memcpy(base + dest.o, src, len);
	}
	void memcpy(void* dest, guest_ptr src, size_t len) const {
		::memcpy(dest, base + src.o, len);
	}
	void memset(guest_ptr dest, char d, size_t len) {
		::memset(base + dest.o, d, len);
	}
	int strlen(guest_ptr p) const {
		return ::strlen(base + p.o);
	}
	
	/* virtual memory handling, these update the mappings as 
	   necessary and also do the proper protection to 
	   distinguish between self-modifying/generating code
	   and normal data writes */
	int mmap(guest_ptr& result, guest_ptr addr, size_t length,
	 	int prot, int flags, int fd, off_t offset);
	int mprotect(guest_ptr offset, size_t length, 
		int prot);
	int munmap(guest_ptr offset, size_t length);
	int mremap(
		guest_ptr& result, guest_ptr old_offset, 
		size_t old_length, size_t new_length,
		int flags, guest_ptr new_offset);

private:
	bool sbrkInitial();
	bool sbrkInitial(guest_ptr new_top);

	typedef std::map<guest_ptr, Mapping> mapmap_t; 
	mapmap_t	maps;
	guest_ptr	top_brick;
	guest_ptr	base_brick;
	bool		is_32_bit;
	guest_ptr	reserve_brick;
	char*		base;
	bool		force_flat;
};

#endif
