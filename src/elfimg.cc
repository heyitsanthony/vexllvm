#include <assert.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#include "Sugar.h"
#include "elfimg.h"
#include "elfsegment.h"

ElfImg* ElfImg::create(const char* fname)
{
	ElfImg	*e = new ElfImg(fname);

	if (e->isValid()) return e;

	delete e;
	return NULL;
}

ElfImg::~ElfImg(void)
{
	if (fd < 0) return;
	
	munmap(img_mmap, img_bytes_c);
	close(fd);
}

ElfImg::ElfImg(const char* fname)
{
	struct stat	st;

	fd = open(fname, O_RDONLY);
	if (fd == -1) goto err_open;

	if (fstat(fd, &st) == -1) goto err_stat;

	img_bytes_c = st.st_size;
	img_mmap = mmap(NULL, img_bytes_c, PROT_READ, MAP_SHARED, fd, 0);
	if (img_mmap == MAP_FAILED) goto err_mmap;

	hdr = (Elf64_Ehdr*)img_mmap;

	if (!verifyHeader()) goto err_hdr;

	setupSegments();
	applyRelocs();

	/* OK. */
	return;
err_hdr:
	munmap(img_mmap, img_bytes_c);
err_mmap:
err_stat:
	close(fd);
err_open:
	fd = -1;
	return;
}

/* TODO: do mmapping here */
void ElfImg::setupSegments(void)
{
	Elf64_Phdr	*phdr;

	phdr = (Elf64_Phdr*)(((char*)hdr) + hdr->e_phoff);
	for (unsigned int i = 0; i < hdr->e_phnum; i++) {
		ElfSegment	*es;
		
		es = ElfSegment::load(fd, phdr[i]);
		if (!es) continue;
		segments.push_back(es);
	}
}

static const unsigned char ok_ident[EI_NIDENT] = 
	"\x7f""ELF\x2\x1\x1\0\0\0\0\0\0\0\0";

#define EXPECTED(x)	fprintf(stderr, "ELF: expected "x"!\n")

bool ElfImg::verifyHeader(void) const
{
	for (unsigned int i = 0; i < EI_NIDENT; i++) {
		if (hdr->e_ident[i] != ok_ident[i])
			return false;
	}

	if (hdr->e_type != ET_EXEC) {
		EXPECTED("ET_EXEC");
		return false;
	}

	if (hdr->e_machine != EM_X86_64) {
		EXPECTED("EM_X86_64");
		return false;
	}

	/* Kind of slutty. Bad data will kill us.
	 * Add validation tests or just don't care? */
	return true;
}

hostptr_t ElfImg::xlateAddr(elfptr_t elfptr) const
{
	foreach (it, segments.begin(), segments.end()) {
		ElfSegment	*es = *it;
		hostptr_t	ret;

		ret = es->xlate(elfptr);
		if (ret) return ret;
	}

	/* failed to xlate */
	return 0;
}


/* cycle through, apply relocs */
void ElfImg::applyRelocs(void)
{
	Elf64_Shdr	*shdr;

	shdr = (Elf64_Shdr*)(((uintptr_t)img_mmap)+ hdr->e_shoff);
	for (int i = 0; i < hdr->e_shnum; i++) {
		if (shdr[i].sh_type == SHT_RELA) 
			fprintf(stderr, "WARNING: Not relocating RELA '%s'\n",
				getString(shdr[i].sh_name));
		if (shdr[i].sh_type == SHT_REL)
			fprintf(stderr, "WARNING: Not relocating REL '%s'\n",
				getString(shdr[i].sh_name));
	}
}

const char* ElfImg::getString(unsigned int stroff) const
{
	Elf64_Shdr	*shdr;
	const char	*strtab;

	shdr = (Elf64_Shdr*)(((uintptr_t)img_mmap)+ hdr->e_shoff);
	assert (shdr[hdr->e_shstrndx].sh_type == SHT_STRTAB);
	strtab = (const char*)((
		(uintptr_t)img_mmap)+shdr[hdr->e_shstrndx].sh_offset); 
	return &strtab[stroff];
}
