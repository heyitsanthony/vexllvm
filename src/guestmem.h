#ifndef GUESTMEM_H
#define GUESTMEM_H

#include <iostream>
#include <list>
#include <map>

struct guest_ptr {
	guest_ptr() : o(0) {}
	explicit guest_ptr(uintptr_t) : o() {}
	uintptr_t o;
	operator uintptr_t() const {
		return o;
	}
};
template <typename T> 
guest_ptr operator+(const guest_ptr& g, T offset) {
	return guest_ptr(g.o + offset);
}
uintptr_t operator-(const guest_ptr& g, guest_ptr h) {
	return g.o - h.o;
}

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
	char* getBase() const { return base; }
	bool findRegion(size_t len, Mapping& m);

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
};

#endif
