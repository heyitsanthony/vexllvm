#ifndef ELFIMG_H
#define ELFIMG_H

#include <elf.h>
#include <stdio.h>

#include <string>
#include <vector>
#include <map>
#include "collection.h"

#include "symbols.h"

typedef void* hostptr_t;
typedef void* elfptr_t;
typedef const void* celfptr_t;

class ElfSegment;
class DLLib;


namespace Arch {
enum Arch {
	Unknown,
	X86_64,
	ARM,
	I386,
};
}
class ElfImg
{
public:


	static ElfImg* create(const char* fname, bool linked = true);
	virtual ~ElfImg(void);
	hostptr_t xlateAddr(elfptr_t addr) const;
	elfptr_t getEntryPoint(void) const;
	int getHeaderCount() const;
	ElfImg* getInterp(void) const { return interp; }
	bool isDirectMapped(void) const { return direct_mapped; } 
	const char* getFilePath(void) const { return img_path; }
	celfptr_t getHeader() const;
	celfptr_t getBase() const;
	ElfSegment* getFirstSegment() const { return segments.front(); }
	void getSegments(std::list<ElfSegment*>& r) const;
	unsigned int getPageSize() const { return 4096; }
	Arch::Arch getArch() const { return arch; }
private:
	ElfImg(const char* fname, Arch::Arch arch, bool linked);
	static Arch::Arch readHeader(const char* fname, 
		bool require_exe);
	static Arch::Arch readHeader32(const Elf32_Ehdr* hdr, 
		bool require_exe);
	static Arch::Arch readHeader64(const Elf64_Ehdr* hdr, 
		bool require_exe);
	template <typename Elf_Ehdr, typename Elf_Phdr>
	void setupSegments(void);

	char			*img_path;

	PtrList<ElfSegment>	segments;
	void			*img_mmap;
	unsigned int		img_bytes_c;
	int			fd;
	union {
		void		*hdr_raw;
		Elf32_Ehdr	*hdr32;
		Elf64_Ehdr	*hdr64;
	};

	bool direct_mapped;
	ElfImg* interp;
	bool linked;
	unsigned		address_bits;
	std::string		library_root;
	Arch::Arch		arch;
};

#endif
