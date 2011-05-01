#ifndef ELFIMG_H
#define ELFIMG_H

#include <elf.h>
#include <stdio.h>

#include "collection.h"

typedef void* hostptr_t;
typedef void* elfptr_t;

class ElfSegment;

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

	PtrList<ElfSegment>	segments;
	void			*img_mmap;
	unsigned int		img_bytes_c;
	int			fd;
	const Elf64_Ehdr	*hdr;
};

#endif
