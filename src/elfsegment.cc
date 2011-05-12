#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <stdint.h>

#include "elfsegment.h"

/* TODO: use sysconf(_SC_PAGE) */
#define PAGE_SZ		4096
#define page_base(x)	((uintptr_t)(x) & ~((uintptr_t)0xfff))
#define page_off(x)	((uintptr_t)(x) & ((uintptr_t)0xfff))
#define page_round_up(x) PAGE_SZ*(((uintptr_t)(x) + 0xfff)/PAGE_SZ)


ElfSegment* ElfSegment::load(int fd, const Elf64_Phdr& phdr)
{
	/* in the future we might care about non-loadable segments */
	if (phdr.p_type != PT_LOAD) return NULL;

	/* FIXME might cause trouble if expecting 0's */
	if (phdr.p_memsz != phdr.p_filesz) {
		fprintf(stderr, 
			"WARNING: PHDR@%p memsz != filesz.\n",
			(void*)phdr.p_vaddr);
	}
	
	return new ElfSegment(fd, phdr);
}

/* Always mmap in the segment. We don't give a fuck. 
 * TODO: Efficiently use the address space! */
ElfSegment::ElfSegment(int fd, const Elf64_Phdr& phdr)
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

void ElfSegment::makeMapping(int fd, const Elf64_Phdr& phdr)
{
	off_t	file_off_pgbase, file_off_pgoff;
	void	*desired_base, *spill_base;
	size_t	end_off;
	int	prot, flags;

	/* map in */
	file_off_pgbase = page_base(phdr.p_offset);
	file_off_pgoff = page_off(phdr.p_offset);

	prot = PROT_READ;
	if (phdr.p_flags & PF_W) prot |= PROT_WRITE;
	flags = (prot & PROT_WRITE) ? MAP_PRIVATE  : MAP_SHARED;

	desired_base = (void*)page_base(phdr.p_vaddr);
	es_len = page_round_up(phdr.p_vaddr + phdr.p_memsz);
	es_len -= page_base(phdr.p_vaddr);

	end_off = page_round_up(phdr.p_offset + phdr.p_memsz);
	if (end_off > page_round_up(elf_file_size))
		spill_pages  = end_off - page_round_up(elf_file_size);
	else
		spill_pages = 0;
	file_pages = es_len - spill_pages;

	es_mmapbase = mmap(
		desired_base, file_pages, prot, flags, fd, file_off_pgbase);
	assert (es_mmapbase != MAP_FAILED);
	direct_mapped = (desired_base == es_mmapbase);

	/* declare guest-visible mapping */
	es_elfbase = (void*)phdr.p_vaddr;
	es_hostbase = (void*)(((char*)es_mmapbase)+file_off_pgoff);

	if (spill_pages == 0)
		return;

	desired_base = (void*)(((char*)desired_base) + file_pages);
	spill_base = mmap(
		desired_base, spill_pages, prot, 
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	assert (spill_base != MAP_FAILED);
	direct_mapped = direct_mapped && (desired_base == spill_base);
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
