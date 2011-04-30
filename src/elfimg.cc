#include <assert.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#include "elfimg.h"

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

void ElfImg::setupSegments(void)
{
	Elf64_Phdr	*phdr;

	phdr = (Elf64_Phdr*)(((char*)hdr) + hdr->e_phoff);
	for (unsigned int i = 0; i < hdr->e_phnum; i++) {
		struct elf_ptrmap	epm;

		if (phdr[i].p_type != PT_LOAD) continue;

		/* FIXME might cause trouble if expecting 0's */
		if (phdr[i].p_memsz != phdr[i].p_filesz) {
			fprintf(stderr, 
				"WARNING: PHDR[%d] memsz != filesz\n",
				i);
		}

		epm.epm_elfbase = (void*)phdr[i].p_vaddr;
		epm.epm_hostbase = (void*)(((char*)img_mmap)+phdr[i].p_offset);
		epm.epm_len = phdr[i].p_memsz;
		segments.push_back(epm);
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

	return true;
}

hostptr_t ElfImg::xlateAddr(elfptr_t elfptr) const
{
	uintptr_t	elfptr_i = (uintptr_t)elfptr;

	for (unsigned int i = 0; i < segments.size(); i++) {
		uintptr_t	seg_base = (uintptr_t)segments[i].epm_elfbase;
		unsigned int	seg_len = segments[i].epm_len;
		uintptr_t	off;

		if ((seg_base > elfptr_i) || ((seg_base + seg_len) < elfptr_i))
			continue;

		assert (elfptr_i >= seg_base);
		off = elfptr_i - seg_base;
		return (void*)(off + (uintptr_t)segments[i].epm_hostbase);
	}

	/* failed to xlate */
	return 0;
}
