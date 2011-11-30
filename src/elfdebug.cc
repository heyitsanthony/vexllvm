#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include "arch.h"
#include "symbols.h"
#include "guestmem.h"
#include "elfimg.h"
#include "elfdebug.h"

#include <stdio.h>

Symbols* ElfDebug::getSyms(const char* elf_path, uintptr_t base)
{
	Symbols		*ret;
	Symbol		*s;
	ElfDebug	*ed;

	ed = new ElfDebug(elf_path);
	if (ed->is_valid == false) {
		delete ed;
		return NULL;
	}

	ret = new Symbols();
	while ((s = ed->nextSym()) != NULL) {
		symaddr_t	addr;

		addr = s->getBaseAddr();
		if (s->isCode() && s->getName().size() > 0 && addr) {
			if (s->isDynamic()) addr += base;
			ret->addSym(s->getName(), addr, s->getLength());
		}
		delete s;
	}

	delete ed;
	return ret;
}

Symbols* ElfDebug::getLinkageSyms(
	const GuestMem* m, const char* elf_path)
{
	Symbols		*ret;
	Symbol		*s;
	ElfDebug	*ed;

	ed = new ElfDebug(elf_path);
	if (ed->is_valid == false) {
		delete ed;
		return NULL;
	}

	ret = new Symbols();
	while ((s = ed->nextLinkageSym(m)) != NULL) {
		ret->addSym(s->getName(), s->getBaseAddr(), s->getLength());
		delete s;
	}

	delete ed;
	return ret;

}

ElfDebug::ElfDebug(const char* path)
: is_valid(false)
, rela_tab(NULL)
, dynsymtab(NULL)
{
	Arch::Arch	elf_arch;
	struct stat	s;
	int		err;

	elf_arch = ElfImg::getArch(path);
	if (elf_arch == Arch::Unknown) return;

	fd = open(path, O_RDONLY);
	if (fd < 0) return;

	err = fstat(fd, &s);
	assert (err == 0 && "Unexpected bad fstat");

	img_byte_c = s.st_size;
	img = (char*)mmap(NULL, img_byte_c, PROT_READ, MAP_SHARED, fd, 0);
	assert (img != MAP_FAILED && "Couldn't map elf ANYWHERE?");

	/* plow through headers */
	setupTables<Elf64_Ehdr, Elf64_Shdr, Elf64_Sym>();

	is_valid = true;
}

ElfDebug::~ElfDebug(void)
{
	if (!is_valid) return;
	munmap(img, img_byte_c);
	close(fd);
}

template <typename Elf_Ehdr, typename Elf_Shdr, typename Elf_Sym>
void ElfDebug::setupTables(void)
{
	Elf_Ehdr	*hdr;
	Elf_Shdr	*shdr;
	const char	*strtab_sh;

	hdr = (Elf_Ehdr*)img;
	shdr = (Elf_Shdr*)(img + hdr->e_shoff);
	strtab_sh = (const char*)(img + shdr[hdr->e_shstrndx].sh_offset);

	/* pull data from section headers */
	strtab = NULL;
	dynstrtab = NULL;
	dynsymtab = NULL;
	symtab = NULL;
	sym_count = 0;
	rela_tab = NULL;

	for (int i = 0; i < hdr->e_shnum; i++) {
		if (i == hdr->e_shstrndx) continue;

		if (shdr[i].sh_type == SHT_DYNSYM) {
			dynsymtab = (Elf_Sym*)(img + shdr[i].sh_offset);
			dynsym_count = shdr[i].sh_size / shdr[i].sh_entsize;
			continue;
		}

		if (	shdr[i].sh_type == SHT_STRTAB &&
			strcmp(&strtab_sh[shdr[i].sh_name], ".dynstr") == 0)
		{
			dynstrtab = (const char*)(img + shdr[i].sh_offset);
			continue;
		}

		if (shdr[i].sh_type == SHT_STRTAB) {
			strtab = (const char*)(img + shdr[i].sh_offset);
			continue;
		}


		if (shdr[i].sh_type == SHT_SYMTAB) {
			symtab = (void*)(img + shdr[i].sh_offset);
			sym_count = shdr[i].sh_size / shdr[i].sh_entsize;
			assert (sizeof(Elf_Sym) == shdr[i].sh_entsize);
			continue;
		}

		if (	shdr[i].sh_type == SHT_RELA && 
			shdr[i].sh_info == 12 /* XXX ??? */)
		{
			rela_tab = (void*)(img + shdr[i].sh_offset);
			rela_count = shdr[i].sh_size / shdr[i].sh_entsize;
			continue;
		}

	}

	if (symtab == NULL) {
		symtab = dynsymtab;
		sym_count = dynsym_count;
		strtab = dynstrtab;
		is_reloc = false;
	} else {
		is_reloc = true;
	}

	next_sym_idx = 0;
	next_rela_idx = 0;

	/* missing some data we expect; don't try to grab any symbols */
	if (symtab == NULL || strtab == NULL)
		sym_count = 0;
}

Symbol* ElfDebug::nextSym(void)
{
	Elf64_Sym	*sym = (Elf64_Sym*)symtab;	/* FIXME */
	Elf64_Sym	*cur_sym;
	const char	*name_c, *atat;
	std::string	name;

	if (next_sym_idx >= sym_count)
		return NULL;

	cur_sym = &sym[next_sym_idx++];

	name_c = &strtab[cur_sym->st_name];
	name = std::string(name_c);
	atat = strstr(name_c, "@@");
	if (atat) {
		name = name.substr(0, atat - name_c);
	}

	return new Symbol(
		name,
		cur_sym->st_value,
		cur_sym->st_size,
		is_reloc,
		(ELF64_ST_TYPE(cur_sym->st_info) == STT_FUNC));
}

Symbol* ElfDebug::nextLinkageSym(const GuestMem* m)
{
	Elf64_Sym	*cur_sym;
	guest_ptr	guest_sym;
	Elf64_Sym	*sym = (Elf64_Sym*)dynsymtab;
	Elf64_Rela	*rela;
	const char	*name_c;

	if (!rela_tab || next_rela_idx >= rela_count)
		return NULL;

	rela = &((Elf64_Rela*)rela_tab)[next_rela_idx++];
	cur_sym = &sym[ELF64_R_SYM(rela->r_info)];
	name_c = &dynstrtab[cur_sym->st_name];
	guest_sym = guest_ptr(rela->r_offset);

	return new Symbol(
		name_c,
		m->read<uint64_t>(guest_sym)-6,
		6,
		false,
		(ELF64_ST_TYPE(cur_sym->st_info) == STT_FUNC));

}
