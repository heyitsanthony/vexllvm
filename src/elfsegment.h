#ifndef ELFSEG_H
#define ELFSEG_H

#include "elfimg.h"

class ElfSegment
{
public:
	virtual ~ElfSegment(void);
	template <typename Elf_Phdr>
	static ElfSegment* load(int fd, const Elf_Phdr& phdr, 
		elfptr_t reloc);
	hostptr_t xlate(elfptr_t elfaddr) const;
	elfptr_t relocation() const { 
		return (void*)((uintptr_t)es_hostbase -
			(uintptr_t)es_elfbase); 
	};
	hostptr_t offset(uintptr_t offset) {
		return (void *)((char*)es_hostbase + offset);
	}
	hostptr_t base() const {
		return es_mmapbase;
	}
	unsigned int length() const {
		return es_len;
	}
	int protection() const {
		return prot;
	}
	void clearEnd() {
		memset((char*)my_end, 0, extra_bytes);
	}
	bool isDirectMapped(void) const { return direct_mapped; }
protected:
	template <typename Elf_Phdr>
	ElfSegment(int fd, const Elf_Phdr& phdr,
		elfptr_t reloc);
private:
	template <typename Elf_Phdr>
	void makeMapping(int fd, const Elf_Phdr& phdr);
	void statFile(int fd);

	elfptr_t	es_elfbase;
	hostptr_t	es_hostbase;
	void		*es_mmapbase;
	unsigned int	es_len;
	unsigned int	file_pages;
	unsigned int	spill_pages;

	bool		direct_mapped;

	size_t		elf_file_size;
	elfptr_t	reloc;
	int		prot;
	elfptr_t	my_end;
	unsigned int	extra_bytes;
};

#endif
