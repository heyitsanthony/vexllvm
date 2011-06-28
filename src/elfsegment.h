#ifndef ELFSEG_H
#define ELFSEG_H

#include "elfimg.h"
#include "guestmem.h"

class ElfSegment
{
public:
	virtual ~ElfSegment(void);
	template <typename Elf_Phdr>
	static ElfSegment* load(GuestMem* mem, int fd, const Elf_Phdr& phdr, 
		uintptr_t reloc);
	guest_ptr xlate(guest_ptr elfaddr) const;
	uintptr_t relocation() const { return es_hostbase - es_elfbase; }
	guest_ptr offset(uintptr_t offset) { return es_hostbase + offset; }
	guest_ptr base() const { return es_mmapbase; }
	unsigned int length() const { return es_len; }
	int protection() const { return prot; }
	void clearEnd() { mem->memset(my_end, 0, extra_bytes); }

protected:
	template <typename Elf_Phdr>
	ElfSegment(GuestMem* mem, int fd, const Elf_Phdr& phdr,
		uintptr_t reloc);
private:
	template <typename Elf_Phdr>
	void makeMapping(int fd, const Elf_Phdr& phdr);
	void statFile(int fd);

	guest_ptr	es_elfbase;
	guest_ptr	es_hostbase;
	guest_ptr	es_mmapbase;
	unsigned int	es_len;
	unsigned int	file_pages;
	unsigned int	spill_pages;

	size_t		elf_file_size;
	guest_ptr	reloc;
	int		prot;
	guest_ptr	my_end;
	unsigned int	extra_bytes;
	GuestMem	*mem;
};

#endif
