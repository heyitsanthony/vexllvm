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

#include "guest.h"
#include "guestcpustate.h"
#include "elfcore.h"

#include <vector>

#define WARNING(x)	fprintf(stderr, "WARNING: "x"\n")


extern "C" { extern uint64_t LibVEX_GuestAMD64_get_rflags(const void* s); }

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


template <typename Elf_Nhdr>
static unsigned writeNote(std::ostream& os, int ty, const void* b, unsigned b_c)
{
	Elf_Nhdr	nhdr;

	nhdr.n_namesz = 5;
	nhdr.n_descsz = b_c;
	nhdr.n_type = ty;
	os.write((char*)&nhdr, sizeof(nhdr));
	os.write("CORE\0\0\0\0", 8); /* lol */
	os.write((const char*)b, b_c);
	
	return sizeof(nhdr) + nhdr.n_namesz + nhdr.n_descsz;
}

template <typename Elf_Ehdr, typename Elf_Phdr, typename Elf_Nhdr>
static void writeElfCore(const Guest *gs, std::ostream& os)
{
	unsigned	map_c, load_off, note_off, note_sz, slack;
	std::list<GuestMem::Mapping>	maps(gs->getMem()->getMaps());
	Elf_Ehdr	ehdr;
	Elf_Phdr	phdr;
	Arch::Arch	arch;


	struct _libc_fpstate	fpr;
	/* stuff for NOTE section */
	prstatus_t	prs;
	prpsinfo_t	prps;

	memset(&prs, 0, sizeof(prs));
	memset(&prps, 0, sizeof(prps));

	memset(&ehdr.e_ident, 0, sizeof(ehdr.e_ident));
	strcpy((char*)&ehdr.e_ident, "\x7f\x45\x4c\x46\x02\x01\x01");

	arch = gs->getArch();
	ehdr.e_type = ET_CORE;
	switch (arch) {
	case Arch::I386:
		ehdr.e_machine = EM_386;
		ehdr.e_ident[EI_CLASS] = ELFCLASS32;
#if 0
		prs.pr_reg[0] = GS;
		prs.pr_reg[1] = FS;
		prs.pr_reg[2] = ES;
		prs.pr_reg[3] = DS;
		prs.pr_reg[4] = EDI;
		prs.pr_reg[5] = ESI;
		prs.pr_reg[6] = EBP;
		prs.pr_reg[7] = ESP;
		prs.pr_reg[8] = EBX;
		prs.pr_reg[9] = EDX;
		prs.pr_reg[10] = ECX;
		prs.pr_reg[11] = TRAPNO;
		prs.pr_reg[12] = ERR;
		prs.pr_reg[13] = EIP;
		prs.pr_reg[14] = EFL;
		prs.pr_reg[15] = UESP;
		prs.pr_reg[16] = SS;
#endif
		assert (0 == 1 && "STUB");
		abort();
		break;

	case Arch::X86_64:
		ehdr.e_machine = EM_X86_64;
		ehdr.e_ident[EI_CLASS] = ELFCLASS64;
	
#define GET_REG(x,y,z) gs->getCPUState()->getReg(x, y, z)
#define SET_PRREG(i,x) prs.pr_reg[i] = gs->getCPUState()->getReg(x, 64)
		
		/* taken from elfutils prstatus_regs */
		SET_PRREG(0, "R15");
		SET_PRREG(1, "R14");
		SET_PRREG(2, "R13");
		SET_PRREG(3, "R12");
		SET_PRREG(4, "RBP");
		SET_PRREG(5, "RBX");
		SET_PRREG(6, "R11");
		SET_PRREG(7, "R10");
		SET_PRREG(8, "R9");
		SET_PRREG(9, "R8");
		SET_PRREG(10, "RAX");
		SET_PRREG(11, "RCX");
		SET_PRREG(12, "RDX");
		SET_PRREG(13, "RSI");
		SET_PRREG(14, "RDI");
		SET_PRREG(16, "RIP");
		SET_PRREG(19, "RSP");

		SET_PRREG(21, "FS_ZERO");
		prs.pr_reg[18] = LibVEX_GuestAMD64_get_rflags(
			gs->getCPUState()->getStateData());
		// SET_PRREG(17, "CSFSGS");
		WARNING("core not setting CSFSGS");
		// SET_PRREG(18, "TRAPNO");
		WARNING("core not setting TRAPNO");
		// SET_PRREG(19, "OLDMASK");
		WARNING("core not setting CR2");
		// SET_PRREG(20, "CR2");


		memset(&fpr, 0, 0);
#if 0
/* UHH?? */
		fpr.cwd
		fpr.swd
		fpr.ftw
		fpr.fop
		fpr.rip
		fpr.rdp
		fpr.mxcsr
		fpr.mxcr_mask
		fpr._st[...];
#endif
		for (int i = 0; i < 16; i++) {
			uint64_t *v = (uint64_t*)(((char*)&fpr) + (32 + 128 + i*2*8));
			v[1] = GET_REG("YMM", 64, i*4);
			v[0] = GET_REG("YMM", 64, i*4 + 1);
		}
		break;
	default:
		std::cerr << "ARCH = " << arch << '\n';
		assert (0 == 1 && "WAT??");
		abort();
	}

	ehdr.e_ident[EI_VERSION] = EV_CURRENT;
	ehdr.e_ident[EI_OSABI] = ELFOSABI_SYSV; /* XXX */

	ehdr.e_version = EV_CURRENT;
	ehdr.e_entry = 0;
	ehdr.e_phoff = sizeof(ehdr);
	ehdr.e_shoff = 0;
	ehdr.e_flags = 0;
	ehdr.e_ehsize = sizeof(ehdr);
	ehdr.e_phentsize = sizeof(Elf_Phdr);
	ehdr.e_shentsize = 0;
	ehdr.e_shnum = 0;
	ehdr.e_shstrndx = 0;


	map_c = maps.size();
	ehdr.e_phnum = map_c + 1 /* note section */;

	os.write((char*)&ehdr, sizeof(ehdr));

	note_off = ehdr.e_phoff + ehdr.e_phnum*sizeof(Elf_Phdr);
	note_sz = 3*(sizeof(Elf_Nhdr)+8) + sizeof(prs) + sizeof(prps) + sizeof(fpr);

	/* the NOTE phdr */
	phdr.p_type = PT_NOTE;
	phdr.p_offset = note_off; 
	phdr.p_vaddr = 0;
	phdr.p_paddr = 0;
	phdr.p_filesz = note_sz;
	phdr.p_memsz = 0;
	phdr.p_flags = 0;
	phdr.p_align = 0;

	os.write((char*)&phdr, sizeof(phdr));

	/* the LOAD phdrs */
	phdr.p_type = PT_LOAD;
	phdr.p_paddr = 0;
	phdr.p_offset = note_off + note_sz; 
	phdr.p_align = 0x1000;

	load_off = note_off + note_sz;
	slack = (0x1000*(((load_off)+(0xfff))/0x1000))-(load_off);
	load_off += slack;


	unsigned			load_sz = 0;

	foreach (it, maps.begin(), maps.end()) {
		GuestMem::Mapping	gm(*it);
		int			prot = gm.getCurProt();

		phdr.p_vaddr = gm.offset.o;
		phdr.p_filesz = gm.length;
		phdr.p_memsz = gm.length;
		phdr.p_offset = load_off + load_sz; 
		phdr.p_flags =
			((prot & PROT_READ) ? PF_R : 0) |
			((prot & PROT_WRITE) ? PF_W : 0) |
			((prot & PROT_EXEC) ? PF_X : 0);

		load_sz += gm.length;
		os.write((char*)&phdr, sizeof(phdr));
	}

	// NT_PRSTATUS
	/* XXX: this needs to be filled out with actual siginfo
	 * on a fault (SIGSEGV, SIGFPE) */
	prs.pr_pid = getpid();
	prs.pr_ppid = getppid();
	prs.pr_pgrp = getpgrp();
	prs.pr_sid = getsid(0);
	prs.pr_fpvalid = 1;

	/* XXX: this needs to be adapted to work for all
	 * kinds of exceptions.. maybe writeCore should
	 * have a signal struct param? */
	prs.pr_info.si_signo = 11;
	prs.pr_info.si_code = 0;
	prs.pr_info.si_errno = 0;
	prs.pr_cursig  = 11;

	writeNote<Elf_Nhdr>(os, NT_PRSTATUS, &prs, sizeof(prs));

	// NT_PRPSINFO
	//
	memset(&prps, 0, sizeof(prps));
	prps.pr_uid = getuid();
	prps.pr_gid = getgid();
	prps.pr_pid = getpid();
	prps.pr_ppid = getppid();
	prps.pr_pgrp = getpgrp();
	prps.pr_sid = getsid(0);
	prps.pr_fname[0] = 'e';
	/* XXX: need to put argvs in here */
	// prs.pr_psargs

	writeNote<Elf_Nhdr>(os, NT_PRPSINFO, &prps, sizeof(prps));
	writeNote<Elf_Nhdr>(os, NT_FPREGSET, &fpr, sizeof(fpr));

	/* hurr */
	for (unsigned i = 0; i < slack; i++) os.put('\0');

	/* and all the rest! */
	foreach (it, maps.begin(), maps.end()) {
		GuestMem::Mapping	gm(*it);
		char			buf[4096];

		for (unsigned i = 0; i < (gm.length + 4095) / 4096; i++) {
			gs->getMem()->memcpy(
				buf,
				gm.offset + 4096*i,
				4096);
			os.write(buf, 4096);
		}
	}
}

void ElfImg::writeCore(const Guest *gs, std::ostream& os)
{
	Arch::Arch arch(gs->getArch());

	switch (arch) {
	case Arch::I386:
		writeElfCore<Elf32_Ehdr, Elf32_Phdr, Elf32_Nhdr>(gs, os);
		break;
	case Arch::X86_64:
		writeElfCore<Elf64_Ehdr, Elf64_Phdr, Elf64_Nhdr>(gs, os);
		break;
	default:
		assert (0 ==1 && "NOT SUPPORTED");
		abort();
	}
}
