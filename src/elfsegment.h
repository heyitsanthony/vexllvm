#ifndef ELFSEG_H
#define ELFSEG_H

#include "elfimg.h"

class ElfSegment
{
public:
	virtual ~ElfSegment(void);
	static ElfSegment* load(int fd, const Elf64_Phdr& phdr);
	hostptr_t xlate(elfptr_t elfaddr) const;
	bool isDirectMapped(void) const { return direct_mapped; }
protected:
	ElfSegment(int fd, const Elf64_Phdr& phdr);
private:
	elfptr_t	es_elfbase;
	hostptr_t	es_hostbase;
	void		*es_mmapbase;
	unsigned int	es_len;
	bool		direct_mapped;
};

#endif
