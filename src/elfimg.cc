#include <assert.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "Sugar.h"
#include "dllib.h"
#include "elfimg.h"
#include "elfsegment.h"

#include <vector>

#define WARNING(x)	fprintf(stderr, "WARNING: "x)

ElfImg* ElfImg::create(const char* fname)
{
	ElfImg	*e = new ElfImg(fname, true);

	if (e->isValid()) return e;
	delete e;
	return NULL;
}

ElfImg* ElfImg::createUnlinked(const char* fname)
{
	ElfImg	*e = new ElfImg(fname, false);

	if (e->isValid()) return e;
	delete e;
	return NULL;
}


ElfImg::~ElfImg(void)
{
	free(img_path);

	if (fd < 0) return;
	
	munmap(img_mmap, img_bytes_c);
	close(fd);
}

ElfImg::ElfImg(const char* fname, bool linked)
{
	struct stat	st;

	img_path = strdup(fname);

	fd = open(fname, O_RDONLY);
	if (fd == -1) goto err_open;

	if (fstat(fd, &st) == -1) goto err_stat;

	img_bytes_c = st.st_size;
	img_mmap = mmap(NULL, img_bytes_c, PROT_READ, MAP_SHARED, fd, 0);
	if (img_mmap == MAP_FAILED) goto err_mmap;

	hdr = (Elf64_Ehdr*)img_mmap;
	shdr_tab = (Elf64_Shdr*)(((uintptr_t)img_mmap)+ hdr->e_shoff);

	if (!verifyHeader()) goto err_hdr;

	setupSegments();

	if (linked) {
		loadDyn();
		applyRelocs();
	}

	/* OK. */
	return;
	/* ERR */
err_hdr:
	munmap(img_mmap, img_bytes_c);
err_mmap:
err_stat:
	close(fd);
err_open:
	fd = -1;
	return;
}

/* TODO: do mmapping here */
void ElfImg::setupSegments(void)
{
	Elf64_Phdr	*phdr;

	direct_mapped = true;
	phdr = (Elf64_Phdr*)(((char*)hdr) + hdr->e_phoff);
	for (unsigned int i = 0; i < hdr->e_phnum; i++) {
		ElfSegment	*es;
		
		es = ElfSegment::load(fd, phdr[i]);
		if (!es) continue;
		if (!es->isDirectMapped()) direct_mapped = false;

		segments.push_back(es);
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

	/* Kind of slutty. Bad data will kill us.
	 * Add validation tests or just don't care? */
	return true;
}

hostptr_t ElfImg::xlateAddr(elfptr_t elfptr) const
{
	foreach (it, segments.begin(), segments.end()) {
		ElfSegment	*es = *it;
		hostptr_t	ret;

		ret = es->xlate(elfptr);
		if (ret) return ret;
	}

	/* failed to xlate */
	return 0;
}

void ElfImg::applyRelaSection(const Elf64_Shdr& shdr)
{
	const Elf64_Rela	*rela;
	unsigned int		rela_c;

	rela_c = getSectElems(&shdr);
	rela = (const Elf64_Rela*)getSectPtr(&shdr);

	for (unsigned int i = 0; i < rela_c; i++)
		applyRela(rela[i]);
}

void ElfImg::applyRela(const Elf64_Rela& rela)
{
	hostptr_t	hostptr;
	int		r_sym;
	int		r_type;
	const Elf64_Sym	*dynsym;
	const char	*symname;
	Elf64_Xword	st_value;

	hostptr = xlateAddr((elfptr_t)rela.r_offset);
	if (hostptr == NULL) {
		fprintf(stderr,
			"BAD HOSTPTR. rela."
			"r_offset=%lx r_info=%lx r_addend=%ld\n",
			rela.r_offset, rela.r_info, rela.r_addend);
		return;
	}

	assert (rela.r_addend == 0 && "NONZERO ADDEND??");
	r_sym = ELF64_R_SYM(rela.r_info);
	r_type = ELF64_R_TYPE(rela.r_info);
	dynsym = getSym(r_sym);
	symname = getDynStr(dynsym->st_name);

	st_value = (Elf64_Addr)getLinkValue(symname);

	switch (r_type) {
	case R_X86_64_GLOB_DAT:
		if (strcmp(symname, "__gmon_start__") == 0) {
			WARNING("faking GLOB_DAT __gmon_start__\n");
		}
		*((Elf64_Addr*)hostptr) = st_value + rela.r_addend;
		break;
	case R_X86_64_COPY:
		/* calculation: none */
//		*((uintptr_t*)st_value) = (uintptr_t)st_value;
//		*((uintptr_t*)st_value) = (uintptr_t)hostptr;
		memcpy((void*)hostptr, (void*)st_value, dynsym->st_size);
		break;
	case R_X86_64_JUMP_SLOT:
		if (st_value == 0) {
			fprintf(stderr, 
				"WARNING: JUMP_SLOT %s with st_value=0\n",
				symname);
		}
		assert (st_value && "OOOPS");
		*((Elf64_Addr*)hostptr) = st_value + rela.r_addend;
		break;
	default:
		fprintf(stderr, "UNHANDLED RELA TYPE %d.\n", r_type);
	}
}

/* cycle through, apply relocs */
void ElfImg::applyRelocs(void)
{
	for (int i = 0; i < hdr->e_shnum; i++) {
		switch(shdr_tab[i].sh_type) {
		case SHT_RELA: applyRelaSection(shdr_tab[i]); break;
		case SHT_REL:
			fprintf(stderr, "WARNING: Not relocating REL '%s'\n",
				getStr(shdr_tab[i].sh_name));
			break;
		default: break;
		}
	}
}

const char* ElfImg::getStr(unsigned int stroff) const
{
	const char	*strtab;

	assert (shdr_tab[hdr->e_shstrndx].sh_type == SHT_STRTAB);
	strtab = (const char*)((
		(uintptr_t)img_mmap)+shdr_tab[hdr->e_shstrndx].sh_offset); 
	return &strtab[stroff];
}

const Elf64_Sym* ElfImg::getSym(unsigned int symidx) const
{
	assert (symidx < dynsym_c && "SYMIDX OUT OF BOUNDS");
	return &dynsym_tab[symidx];
}

const Elf64_Shdr* ElfImg::getDynShdr(void) const
{
	for (unsigned int i = 0; i < hdr->e_shnum; i++) {
		if (shdr_tab[i].sh_type == SHT_DYNAMIC)
			return  &shdr_tab[i];
	}

	return NULL;
}

hostptr_t* ElfImg::getSectPtr(const Elf64_Shdr* shdr) const
{
	return (hostptr_t*)((uintptr_t)img_mmap + shdr->sh_offset);
}

void ElfImg::loadDyn(void)
{
	const Elf64_Shdr *dyn_shdr = NULL;
	const Elf64_Dyn	*dyntab;
	unsigned int	dyntab_c;

	/* find matching shdr */
	dyn_shdr = getDynShdr();
	for (unsigned int i = 0; i < hdr->e_shnum; i++) {
		if (shdr_tab[i].sh_type == SHT_DYNSYM) {
			dynsym_shdr = &shdr_tab[i];
			dynsym_tab = (const Elf64_Sym*)getSectPtr(dynsym_shdr);
			dynsym_c = getSectElems(dynsym_shdr);
		}
	}

	assert (dyn_shdr != NULL && "Expected dynanmic section");
	dyntab = (const Elf64_Dyn*)(getSectPtr(dyn_shdr));
	dyntab_c = getSectElems(dyn_shdr);

	std::vector<std::string> needed;
	for (unsigned int i = 0; i < dyntab_c; i++) {
		if (dyntab[i].d_tag == DT_STRTAB) {
			dynstr_tab = (const char*)
				xlateAddr((elfptr_t)dyntab[i].d_un.d_ptr);
			break;
		}
	}

	for (unsigned int i = 0; i < dyntab_c; i++) {
		switch (dyntab[i].d_tag) {
		case DT_NEEDED:
			needed.push_back(
				std::string(getDynStr(dyntab[i].d_un.d_ptr)));
		break;
		default: break;
		}
	}
	assert (dynstr_tab != NULL);
	assert (dynsym_tab != NULL);
	foreach (it, needed.begin(), needed.end())
		fprintf(stderr, "NEEDED-STR: %s\n", 
		(*it).c_str());

	linkWithLibs(needed);
}

unsigned int ElfImg::getSectElems(const Elf64_Shdr* shdr) const
{
	return shdr->sh_size / shdr->sh_entsize;
}

/* converts dynamic sym in img_mmap to writable dyn segment */
//Elf64_Sym* ElfImg::getSegmentDynSym(const Elf64_Sym* our_sym)
//{
//	assert (0 == 1 && "STUB");
//}

/* Given a library, we go through the execs's symbols looking for
 * matching functions in the lib. */
/* takes ownership of 'lib' from caller */
void ElfImg::linkWith(DLLib* lib)
{
	unsigned int	link_c = 0;
	for (unsigned int i = 0; i < dynsym_c; i++) {
		const char	*fname;
		void		*fptr;

		if (ELF64_ST_BIND(dynsym_tab[i].st_info) != STB_GLOBAL) continue;

		fname = getDynStr(dynsym_tab[i].st_name);
		if (syms.findSym(fname)) continue;

		fptr = lib->resolve(fname);
		if (fptr == NULL) continue;

		/* XXX how do we find len? */
		syms.addSym(fname, (symaddr_t)fptr, 1);
		link_c++;
	}

	if (!link_c) 
		delete lib;
	else
		libs.push_back(lib);
}

void* ElfImg::getLinkValue(const char* symname) const
{
	const Symbol	*sym;

	sym = syms.findSym(symname);
	if (sym == NULL) {
		fprintf(stderr, 
			"No link value on %s. Faking it with 0. Good luck.\n",
			symname);
		return NULL;
	}

	return (void*)sym->getBaseAddr();
}

#include <iostream>
/* pull the symbols we want to instrument into the symbol table */
#define INST_CALL_NUM	6
static 	const char* calls[INST_CALL_NUM] = 
{
	"exit",
	"abort",
	"_exit",
	"fork",
	"kill",
	"uselocale"
//		,
//		"exit_group"
};

void ElfImg::pullInstrumented(DLLib* lib)
{
	/* XXX does this belong here? */
	for (unsigned int i = 0; i < INST_CALL_NUM; i++) {
		void	*fptr;
		if (syms.findSym(std::string(calls[i]))) continue;
//		std::cerr << "RESOLVING " << calls[i] << std::endl;
		fptr = lib->resolve(calls[i]);
		if (!fptr) continue;
		syms.addSym(calls[i], (symaddr_t)fptr, 1);
	}
}

/* go through all needed entries */
void ElfImg::linkWithLibs(std::vector<std::string>& needed)
{
	foreach (it, needed.begin(), needed.end()) {
		DLLib	*lib = DLLib::load((*it).c_str());

		if (!lib) {
			fprintf(stderr, 
				"WARNING: Could not dlopen '%s'."
				"Don't expect things to work.\n",
				(*it).c_str());
			continue;
		}

		linkWith(lib);
		pullInstrumented(lib);
	}
}

elfptr_t ElfImg::getSymAddr(const char* symname) const
{
	const Symbol	*sym = syms.findSym(symname);
	return (elfptr_t)((sym) ? sym->getBaseAddr() : (symaddr_t)NULL);
}
