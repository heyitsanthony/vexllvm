#ifndef ELFIMG_H
#define ELFIMG_H

#include <elf.h>

#include <string>
#include <vector>
#include <map>
#include "collection.h"
#include "arch.h"
#include "guestmem.h"

class ElfSegment;
class GuestMem;

/* loads and maps an elf image into memory based on its Phdr--
 * NOTE: two elfimg objects of the same exec can't coexist;
 *       address collision ruins it. */
class ElfImg
{
public:
	static ElfImg* create(const char* fname, bool linked = true);
	virtual ~ElfImg(void);
	guest_ptr xlateAddr(guest_ptr addr) const;
	guest_ptr getEntryPoint(void) const;
	int getHeaderCount() const;
	ElfImg* getInterp(void) const { return interp; }
	const char* getFilePath(void) const { return img_path; }
	const guest_ptr getHeader() const;
	const guest_ptr getBase() const;
	ElfSegment* getFirstSegment() const { return segments.front(); }
	void getSegments(std::list<ElfSegment*>& r) const;
	unsigned int getPageSize() const { return 4096; }
	Arch::Arch getArch() const { return arch; }
	unsigned int getAddressBits() const { return address_bits; }
	unsigned int getElfPhdrSize() const {
		return (address_bits == 32) ?
			sizeof(Elf32_Ehdr) : sizeof(Elf64_Ehdr);
	}
	std::string getLibraryRoot() const { return library_root; }

	static Arch::Arch getArch(const char* fname)
	{
		return readHeader(fname, false);
	}

	GuestMem* takeMem(void)
	{
		GuestMem *m = mem;
		mem = NULL;
		return m;
	}

private:
	ElfImg(const char* fname, Arch::Arch arch, bool linked);
	ElfImg(GuestMem* m, const char* fname, Arch::Arch arch, bool linked);
	void setup(void);

	static ElfImg* create(
		GuestMem* m, const char* fname, bool linked = true);
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

	ElfImg* interp;
	bool linked;
	unsigned		address_bits;
	std::string		library_root;
	Arch::Arch		arch;
	GuestMem		*mem;
	bool			owns_mem;
};

#endif
