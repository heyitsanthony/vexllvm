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
ElfSegment* ElfSegment::load<Elf32_Phdr>(int fd, const Elf32_Phdr& phdr, 
	elfptr_t reloc);
template
ElfSegment* ElfSegment::load<Elf64_Phdr>(int fd, const Elf64_Phdr& phdr, 
	elfptr_t reloc);

template <typename Elf_Phdr>
ElfSegment* ElfSegment::load(int fd, const Elf_Phdr& phdr, 
	elfptr_t reloc)
{
	/* in the future we might care about non-loadable segments */
	if (phdr.p_type != PT_LOAD) 
		return NULL;

	return new ElfSegment(fd, phdr, reloc);
}

template 
ElfSegment::ElfSegment<Elf32_Phdr>(int fd, const Elf32_Phdr& phdr, 
	elfptr_t in_reloc);
template
ElfSegment::ElfSegment<Elf64_Phdr>(int fd, const Elf64_Phdr& phdr, 
	elfptr_t in_reloc);

/* Always mmap in the segment. We don't give a fuck. 
 * TODO: Efficiently use the address space! */
template <typename Elf_Phdr>
ElfSegment::ElfSegment(int fd, const Elf_Phdr& phdr, 
	elfptr_t in_reloc)
: reloc(in_reloc)
{
	statFile(fd);
	makeMapping(fd, phdr);
}

ElfSegment::~ElfSegment(void)
{
	munmap(es_mmapbase, es_len);
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
	off_t	file_off_pgbase, file_off_pgoff;
	void	*desired_base, *spill_base;
	size_t	end_off;
	int	flags;

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

	uintptr_t reloc_base = phdr.p_vaddr + (uintptr_t)reloc;
	desired_base = (void*)page_base(reloc_base);
	es_len = page_round_up(reloc_base + phdr.p_memsz);
	es_len -= page_base(reloc_base);

	end_off = page_round_up(phdr.p_offset + phdr.p_memsz);
	if (end_off > page_round_up(elf_file_size))
		spill_pages  = end_off - page_round_up(elf_file_size);
	else
		spill_pages = 0;
	file_pages = es_len - spill_pages;

	es_mmapbase = mmap(desired_base, file_pages, 
		prot | PROT_WRITE, flags, fd, file_off_pgbase);

	assert (es_mmapbase != MAP_FAILED);
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

	my_end = (void*)((uintptr_t)es_mmapbase + filesz);

	/* protect now that we have zeroed */
	mprotect(es_mmapbase, file_pages, prot);

	/* declare guest-visible mapping */
	es_elfbase = (void*)reloc_base;
	es_hostbase = (void*)(((char*)es_mmapbase)+file_off_pgoff);

	if (spill_pages == 0)
		return;

	desired_base = (void*)(((char*)desired_base) + file_pages);
	spill_base = mmap(
		desired_base, spill_pages, prot, 
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	assert (spill_base != MAP_FAILED);
}

hostptr_t ElfSegment::xlate(elfptr_t elfptr) const
{
	uintptr_t	elfptr_i = (uintptr_t)elfptr;
	uintptr_t	seg_base = (uintptr_t)es_elfbase;
	uintptr_t	off;

	if ((seg_base > elfptr_i) || ((seg_base + es_len) < elfptr_i))
		return 0;

	assert (elfptr_i >= seg_base);
	off = elfptr_i - seg_base;
	return (void*)(off + (uintptr_t)es_hostbase);
}
