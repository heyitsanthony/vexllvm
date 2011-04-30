#ifndef ELFIMG_H
#define ELFIMG_H

#include <elf.h>
#include <stdio.h>

#include <vector>

typedef void* hostptr_t;
typedef void* elfptr_t;

typedef struct elf_ptrmap {
	elfptr_t	epm_elfbase;
	hostptr_t	epm_hostbase;
	unsigned int	epm_len;
} elf_ptrmap;

class ElfImg
{
public:
	static ElfImg* create(const char* fname);
	virtual ~ElfImg(void);
	bool isValid(void) const { return fd > 0; }
	hostptr_t xlateAddr(elfptr_t addr) const;
	elfptr_t getEntryPoint(void) const { return (void*)hdr->e_entry; }
private:
	ElfImg(const char* fname);
	bool verifyHeader(void) const;
	void setupSegments(void);

	std::vector<elf_ptrmap>	segments;
	void			*img_mmap;
	unsigned int		img_bytes_c;
	int			fd;
	const Elf64_Ehdr	*hdr;
};

#endif
