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

#define WARNING(x)	fprintf(stderr, "WARNING: "x)

#define ElfImg32 ElfImg
#define ElfImg64 ElfImg

ElfImg* ElfImg::create(const char* fname, bool linked)
{
	Arch::Arch arch = readHeader(fname, linked);
	if(arch == Arch::Unknown)
		return NULL;
	return new ElfImg(fname, arch, linked);
}

ElfImg* ElfImg::create(GuestMem* m, const char* fname, bool linked)
{
	Arch::Arch arch = readHeader(fname, linked);
	if(arch == Arch::Unknown)
		return NULL;
	return new ElfImg(m, fname, arch, linked);
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
	munmap(img_mmap, img_bytes_c);
	close(fd);
}

ElfImg::ElfImg(const char* fname, Arch::Arch in_arch, bool in_linked)
: interp(NULL)
, linked(in_linked)
, arch(in_arch)
, owns_mem(true)
{
	mem = new GuestMem();
	img_path = strdup(fname);
	setup();
}

ElfImg::ElfImg(
	GuestMem* m,
	const char* fname, Arch::Arch in_arch, bool in_linked)
: interp(NULL)
, linked(in_linked)
, arch(in_arch)
, mem(m)
, owns_mem(false)
{
	assert (mem != NULL);
	img_path = strdup(fname);
	setup();
}

void ElfImg::setup(void)
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

	if(getenv("VEXLLVM_LIBRARY_ROOT"))
		library_root.assign(getenv("VEXLLVM_LIBRARY_ROOT"));

	switch(arch) {
	case Arch::X86_64:
		address_bits = 64;
		setupSegments<Elf64_Ehdr, Elf64_Phdr>();
		break;
	case Arch::ARM:
	case Arch::I386:
		mem->mark32Bit();
		address_bits = 32;
		setupSegments<Elf32_Ehdr, Elf32_Phdr>();
		break;
	default:
		assert(!"unknown arch type");
	}
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
	/* this is so janky and will most likely not work other places */
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
		ElfSegment	*es;

		if (phdr[i].p_type == PT_INTERP) {
			std::string path((char*)img_mmap + phdr[i].p_offset);
			path = library_root + path;
			interp = ElfImg::create(mem, path.c_str(), false);
			continue;
		}
		es = ElfSegment::load(
			mem,
			fd,
			phdr[i],
			segments.empty()
				? uintptr_t(0)
				: getFirstSegment()->relocation());
		if (!es) continue;

		segments.push_back(es);
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

#define EXPECTED(x)	fprintf(stderr, "ELF: expected "x"!\n")

guest_ptr ElfImg::getEntryPoint(void) const
{
	/* address must be translated in case the region was remapped
	   as is the case for the interp which specified a load address
	   base of 0 */
	if(address_bits == 32) {
		return xlateAddr(guest_ptr(hdr32->e_entry));
	} else if(address_bits == 64) {
		return xlateAddr(guest_ptr(hdr64->e_entry));
	} else {
		assert(!"address bits corrupted");
	}
}

Arch::Arch ElfImg::readHeader(const char* fname, bool require_exe)
{
	struct header_cleanup {
		header_cleanup() : fd(-1), data(NULL), size(0) {}
		~header_cleanup() {
			if(data) {
				munmap(data, size);
			}
			if(fd >= 0) {
				close(fd);
			}
		}
		int fd;
		void* data;
		size_t size;
	} header;

	header.fd = open(fname, O_RDONLY);
	if (header.fd == -1) {
		return Arch::Unknown;
	}

	unsigned char ident[IDENT_SIZE];
	int res = read(header.fd, &ident[0], IDENT_SIZE);

	if(res < IDENT_SIZE) {
		return Arch::Unknown;
	}

	unsigned int address_bits = 0;
	if(memcmp(&ok_ident_64[0], &ident[0], IDENT_SIZE) == 0) {
		address_bits = 64;
	} else if(memcmp(&ok_ident_32[0], &ident[0], IDENT_SIZE) == 0) {
		address_bits = 32;
	} else {
		return Arch::Unknown;
	}

	if(sizeof(void*) < address_bits / 8) {
		EXPECTED("Host with matching addressing capabilities");
		return Arch::Unknown;
	}

	header.size = std::max(sizeof(Elf32_Ehdr), sizeof(Elf64_Ehdr));
	header.data = mmap(NULL, header.size, PROT_READ, MAP_PRIVATE,
		header.fd, 0);
	if (header.data == MAP_FAILED) {
		return Arch::Unknown;
	}

	if(address_bits == 32) {
		return readHeader32((Elf32_Ehdr*)header.data, require_exe);
	} else if (address_bits == 64) {
		return readHeader64((Elf64_Ehdr*)header.data, require_exe);
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
	} else if(hdr->e_machine == EM_386) {
		return Arch::I386;
	}

	/* just don't care about the rest, slutty is fun */
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
	foreach (it, segments.begin(), segments.end()) {
		ElfSegment	*es = *it;
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

	foreach (it, segments.begin(), segments.end()) {
		r.push_back(*it);
	}
}

GuestMem* ElfImg::takeMem(void)
{
	GuestMem *m = mem;

	mem = NULL;

	if (interp) interp->takeMem();
	foreach (it, segments.begin(), segments.end()) {
		ElfSegment *es = *it;
		es->takeMem();
	}

	return m;
}
