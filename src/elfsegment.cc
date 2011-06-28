#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "elfsegment.h"

/* TODO: use sysconf(_SC_PAGE) */
#define PAGE_SZ		4096
#define page_base(x)	((uintptr_t)(x) & ~((uintptr_t)0xfff))
#define page_off(x)	((uintptr_t)(x) & ((uintptr_t)0xfff))
#define page_round_up(x) PAGE_SZ*(((uintptr_t)(x) + 0xfff)/PAGE_SZ)


template <typename Elf_Phr>
struct ElfSegmentMapFlags {
	static const int flags = 0;
};

template <>
struct ElfSegmentMapFlags<Elf32_Phdr> {
	static const int flags = MAP_32BIT;
};
template 
ElfSegment* ElfSegment::load<Elf32_Phdr>(GuestMem* mem, int fd, 
	const Elf32_Phdr& phdr, uintptr_t reloc);
template
ElfSegment* ElfSegment::load<Elf64_Phdr>(GuestMem* mem, int fd, 
	const Elf64_Phdr& phdr, uintptr_t reloc);

template <typename Elf_Phdr>
ElfSegment* ElfSegment::load(GuestMem* mem, int fd, const Elf_Phdr& phdr, 
	uintptr_t reloc)
{
	/* in the future we might care about non-loadable segments */
	if (phdr.p_type != PT_LOAD) 
		return NULL;

	return new ElfSegment(mem, fd, phdr, reloc);
}

template 
ElfSegment::ElfSegment<Elf32_Phdr>(GuestMem* mem, int fd, const Elf32_Phdr& phdr, 
	uintptr_t in_reloc);
template
ElfSegment::ElfSegment<Elf64_Phdr>(GuestMem* mem, int fd, const Elf64_Phdr& phdr, 
	uintptr_t in_reloc);

/* Always mmap in the segment. */
template <typename Elf_Phdr>
ElfSegment::ElfSegment(GuestMem* in_mem, int fd, const Elf_Phdr& phdr, 
	uintptr_t in_reloc)
: reloc(in_reloc)
, mem(in_mem)
{
	statFile(fd);
	makeMapping(fd, phdr);
}

ElfSegment::~ElfSegment(void)
{
	mem->munmap(es_mmapbase, es_len);
}

void ElfSegment::statFile(int fd)
{
	struct stat	s;
	fstat(fd, &s);
	elf_file_size = s.st_size;
}

template 
void ElfSegment::makeMapping<Elf32_Phdr>(int fd, const Elf32_Phdr& phdr);
template
void ElfSegment::makeMapping<Elf64_Phdr>(int fd, const Elf64_Phdr& phdr);

template <typename Elf_Phdr>
void ElfSegment::makeMapping(int fd, const Elf_Phdr& phdr)
{
	off_t		file_off_pgbase, file_off_pgoff;
	guest_ptr	desired_base, spill_base;
	size_t		end_off;
	int		flags;

	/* map in */
	file_off_pgbase = page_base(phdr.p_offset);
	file_off_pgoff = page_off(phdr.p_offset);

	prot = PROT_READ;
	if (phdr.p_flags & PF_W) prot |= PROT_WRITE;
	if (phdr.p_flags & PF_X) prot |= PROT_EXEC;
	flags = MAP_PRIVATE;
	flags |= ElfSegmentMapFlags<Elf_Phdr>::flags;
	
	/* TODO: this really isn't cool, but somehow, when the tests
	   are lauched, the memory mapping space is completely shot to
	   hell and it can't manage to map everything at nice addresses! */
	if(phdr.p_vaddr != 0)
		flags |= MAP_FIXED;

	guest_ptr reloc_base = guest_ptr(phdr.p_vaddr) + reloc;
	desired_base = guest_ptr(page_base(reloc_base));
	es_len = page_round_up(reloc_base + phdr.p_memsz);
	es_len -= page_base(reloc_base);

	end_off = page_round_up(phdr.p_offset + phdr.p_memsz);
	if (end_off > page_round_up(elf_file_size))
		spill_pages  = end_off - page_round_up(elf_file_size);
	else
		spill_pages = 0;
	file_pages = es_len - spill_pages;

	int res = mem->mmap(es_mmapbase, desired_base, file_pages, 
		prot | PROT_WRITE, flags, fd, file_off_pgbase);

	assert (res == 0 || "failed to map segment");
	uintptr_t filesz = phdr.p_offset - file_off_pgbase + phdr.p_filesz;
	extra_bytes = file_pages - filesz;

	if(getenv("VEXLLVM_LOG_MAPPINGS")) {
		std::cerr << "mapped section @ " << desired_base 
			<< " to " << es_mmapbase 
			<< " size " << es_len
			<< " file base " << file_off_pgbase 
		<< std::endl;
		/*
		std::cerr << "page_base(reloc_base) " 
			<< (void*)page_base(reloc_base) << std::endl;
		std::cerr << "filesz = " << filesz << std::endl;
		std::cerr << "extra bytes = " << extra_bytes << std::endl;
		*/
	}

	my_end = es_mmapbase + filesz;

	/* protect now that we have zeroed */
	mem->mprotect(es_mmapbase, file_pages, prot);

	/* declare guest-visible mapping */
	es_elfbase = reloc_base;
	es_hostbase = es_mmapbase + file_off_pgoff;

	if (spill_pages == 0)
		return;

	desired_base = desired_base + file_pages;
	res = mem->mmap(spill_base, 
		desired_base, spill_pages, prot, 
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	assert (res == 0 && "failed to map segment spill");
}

guest_ptr ElfSegment::xlate(guest_ptr elfptr) const
{
	uintptr_t	off;

	if ((es_elfbase > elfptr) || ((es_elfbase + es_len) < elfptr))
		return guest_ptr(0);

	assert (elfptr >= es_elfbase);
	off = elfptr - es_elfbase;
	return es_hostbase + off;
}
