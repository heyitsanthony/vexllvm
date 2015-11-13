#include <iostream>
#include <assert.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "Sugar.h"
#include "dllib.h"
#include "elfimg.h"
#include "elfsegment.h"

#include <vector>

#define WARNING(x)	fprintf(stderr, "WARNING: " x "\n")

#define ElfImg32 ElfImg
#define ElfImg64 ElfImg

ElfImg* ElfImg::create(const char* fname, bool linked, bool map_segs)
{
	Arch::Arch arch = readHeader(fname, linked);
	if(arch == Arch::Unknown)
		return NULL;
	return new ElfImg(fname, arch, map_segs);
}

ElfImg* ElfImg::create(GuestMem* m, const char* fname, bool linked, bool map_segs)
{
	Arch::Arch arch = readHeader(fname, linked);
	if(arch == Arch::Unknown)
		return NULL;
	return new ElfImg(m, fname, arch, map_segs);
}

ElfImg::~ElfImg(void)
{
	if (interp) {
		delete interp;
		interp = NULL;
	}
	free(img_path);

	if (fd < 0) return;

	/* segments rely on mem being valid, so we need
	 * to free them before mem is freed */
	segments.clear();

	if (mem != NULL && owns_mem) delete mem;
	if (img_mmap != NULL) munmap(img_mmap, img_bytes_c);
	close(fd);
}

void ElfImg::setupBits(void)
{
	switch(arch) {
	case Arch::X86_64:
		address_bits = 64;
		break;
	case Arch::MIPS32:
	case Arch::ARM:
	case Arch::I386:
		mem->mark32Bit();
		address_bits = 32;
		break;
	default:
		assert(!"unknown arch type");
	}
}

ElfImg::ElfImg(const char* fname, Arch::Arch in_arch, bool map_segs)
: interp(NULL)
, arch(in_arch)
, owns_mem(true)
{
	mem = new GuestMem();
	img_path = strdup(fname);

	setupBits();
	setupImgMMap();
	if (map_segs) setup();
}

ElfImg::ElfImg(
	GuestMem* m,
	const char* fname, Arch::Arch in_arch, bool map_segs)
: interp(NULL)
, arch(in_arch)
, mem(m)
, owns_mem(false)
{
	assert (mem != NULL);
	img_path = strdup(fname);

	setupBits();
	setupImgMMap();
	if (map_segs) setup();
}

void ElfImg::setupImgMMap(void)
{
	struct stat	st;

	fd = open(img_path, O_RDONLY);
	/* should be ok, we did already open it */
	assert(fd >= 0);

	int res = fstat(fd, &st);
	assert(res >= 0);

	img_bytes_c = st.st_size;
	img_mmap = mmap(NULL, img_bytes_c, PROT_READ, MAP_SHARED, fd, 0);
	assert(img_mmap != MAP_FAILED);

	hdr_raw = img_mmap;
}

void ElfImg::setup(void)
{
	if(getenv("VEXLLVM_LIBRARY_ROOT"))
		library_root.assign(getenv("VEXLLVM_LIBRARY_ROOT"));

	if (address_bits == 64)
		setupSegments<Elf64_Ehdr, Elf64_Phdr>();
	else
		setupSegments<Elf32_Ehdr, Elf32_Phdr>();
}

int ElfImg::getHeaderCount() const {
	if(address_bits == 32) {
		return hdr32->e_phnum;
	} else if(address_bits == 64) {
		return hdr64->e_phnum;
	} else {
		assert(!"address bits corrupted");
	}
}

const guest_ptr ElfImg::getHeader() const {
	assert (hdr32 || hdr64);
	if(address_bits == 32) {
		return getFirstSegment()->offset(hdr32->e_phoff);
	} else if(address_bits == 64) {
		return getFirstSegment()->offset(hdr64->e_phoff);
	} else {
		assert(!"address bits corrupted");
	}
}

/* TODO: do mmapping here */
template <typename Elf_Ehdr, typename Elf_Phdr>
void ElfImg::setupSegments(void)
{
	Elf_Ehdr *hdr = (Elf_Ehdr*)hdr_raw;
	Elf_Phdr *phdr = (Elf_Phdr*)(((char*)hdr) + hdr->e_phoff);

	for (unsigned int i = 0; i < hdr->e_phnum; i++) {
		if (phdr[i].p_type == PT_INTERP) {
			std::string path((char*)img_mmap + phdr[i].p_offset);
			path = library_root + path;
			interp = ElfImg::create(mem, path.c_str(), false);
			continue;
		}
		auto es = std::unique_ptr<ElfSegment>(ElfSegment::load(
			mem,
			fd,
			phdr[i],
			segments.empty()
				? uintptr_t(0)
				: getFirstSegment()->relocation()));
		if (!es) continue;

		segments.push_back(std::move(es));
	}
	assert(!segments.empty());
	segments.back()->clearEnd();
}

/*
 * Used to use EI_NIDENT here, but the extra data in the ident field
 * trips things up. On my machine, the OSABI changes from NONE to LINUX on
 * static binaries. This will matter more when we care about other archs.
 *
 * [4] = 2 = ELFCLASS64
 * [5] = 1 = ELFDATA2LSB
 * [6] = 1 = EI_VERSION
 * [7] = 0 = ELFOSABI_NONE / EI_OSABI  XXX
 */
#define IDENT_SIZE EI_VERSION+1
static const unsigned char ok_ident_64[] = "\x7f""ELF\x2\x1\x1";
static const unsigned char ok_ident_32[] = "\x7f""ELF\x1\x1\x1";

#define EXPECTED(x)	fprintf(stderr, "ELF: expected " x "!\n")

guest_ptr ElfImg::getEntryPoint(void) const
{
	/* address must be translated in case the region was remapped
	   as is the case for the interp which specified a load address
	   base of 0 */
	assert (hdr32 || hdr64);

	if (address_bits == 32) {
		return xlateAddr(guest_ptr(hdr32->e_entry));
	} else if (address_bits == 64) {
		return xlateAddr(guest_ptr(hdr64->e_entry));
	} else {
		assert(!"address bits corrupted");
	}
}

Arch::Arch ElfImg::readHeader(const char* fname, bool require_exe)
{ bool x; return readHeader(fname, require_exe, x); }


Arch::Arch ElfImg::readHeader(const char* fname, bool require_exe, bool& is_dyn)
{
	struct header_cleanup {
		header_cleanup() : fd(-1), ident(0) {}
		~header_cleanup() {
			if (ident) delete [] ident;
			if (fd >= 0) close(fd);
		}
		int fd;
		uint8_t* ident;
	} header;
	int	sz, res;

	is_dyn = false;

	header.fd = open(fname, O_RDONLY);
	if (header.fd == -1) return Arch::Unknown;

	sz = std::max(sizeof(Elf32_Ehdr), sizeof(Elf64_Ehdr));
	header.ident = new uint8_t[sz];
	res = read(header.fd, header.ident, sz);
	if (res < sz) return Arch::Unknown;

	return readHeaderMem(header.ident, require_exe, is_dyn);
}

Arch::Arch ElfImg::readHeaderMem(
	const uint8_t* ident,
	bool require_exe,
	bool& is_dyn)
{
	unsigned address_bits = 0;

	if (memcmp(&ok_ident_64[0], &ident[0], IDENT_SIZE) == 0) {
		address_bits = 64;
	} else if (memcmp(&ok_ident_32[0], &ident[0], IDENT_SIZE) == 0) {
		address_bits = 32;
	} else {
		return Arch::Unknown;
	}

	if (sizeof(void*) < address_bits / 8) {
		EXPECTED("Host with matching addressing capabilities");
		return Arch::Unknown;
	}

	if (address_bits == 32) {
		const Elf32_Ehdr	*e32((const Elf32_Ehdr*)ident);
		is_dyn = (e32->e_type == ET_DYN);
		return readHeader32(e32, require_exe);
	} else if (address_bits == 64) {
		const Elf64_Ehdr	*e64((const Elf64_Ehdr*)ident);
		is_dyn = (e64->e_type == ET_DYN);
		return readHeader64(e64, require_exe);
	}

	assert(!"address_bits corrupted");
	return Arch::Unknown;
}

Arch::Arch ElfImg::readHeader32(const Elf32_Ehdr* hdr, bool require_exe)
{
	if (require_exe && hdr->e_type != ET_EXEC) {
		EXPECTED("ET_EXEC");
		return Arch::Unknown;
	}

	if(hdr->e_machine == EM_ARM) {
		return Arch::ARM;
	} else if (hdr->e_machine == EM_386) {
		return Arch::I386;
	} else if (hdr->e_machine == EM_MIPS)
		return Arch::MIPS32;


	return Arch::Unknown;
}

Arch::Arch ElfImg::readHeader64(const Elf64_Ehdr* hdr, bool require_exe)
{
	if (require_exe && hdr->e_type != ET_EXEC) {
		EXPECTED("ET_EXEC");
		return Arch::Unknown;
	}

	if(hdr->e_machine == EM_X86_64) {
		return Arch::X86_64;
	}

	/* just don't care about the rest, slutty is fun */
	return Arch::Unknown;
}

guest_ptr ElfImg::xlateAddr(guest_ptr elfptr) const
{
	if (segments.empty()) return elfptr;

	for (auto &es : segments) {
		guest_ptr	ret;
		ret = es->xlate(elfptr);
		if (ret) return ret;
	}

	/* failed to xlate */
	return guest_ptr(0);
}

void ElfImg::getSegments(std::list<ElfSegment*>& r) const
{
	/* for now i am cheating... the last segment added here will be
	   the value brk returns when it is first called... so order
	   sadly matters... :-( */
	if (interp) interp->getSegments(r);

	for (auto &es : segments) {
		r.push_back(es.get());
	}
}

GuestMem* ElfImg::takeMem(void)
{
	GuestMem *m = mem;

	mem = NULL;

	if (interp) interp->takeMem();
	for (auto &es : segments) es->takeMem();

	return m;
}
