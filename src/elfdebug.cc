#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include "arch.h"
#include "symbols.h"
#include "elfimg.h"
#include "elfdebug.h"

#include <stdio.h>

Symbols* ElfDebug::getSyms(const char* elf_path, void* base)
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
			if (s->isDynamic()) addr += (uint64_t)base;
			ret->addSym(s->getName(), addr, s->getLength());
		}
		delete s;
	}

	delete ed;
	return ret;
}

ElfDebug::ElfDebug(const char* path)
: is_valid(false)
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
	Elf_Sym		*sym;
	const char	*strtab_sh;

	hdr = (Elf_Ehdr*)img;
	shdr = (Elf_Shdr*)(img + hdr->e_shoff);
	strtab_sh = (const char*)(img + shdr[hdr->e_shstrndx].sh_offset);

	/* pull data from section headers */
	strtab = NULL;
	sym_count = 0;
	sym = NULL;
	for (int i = 0; i < hdr->e_shnum; i++) {
		if (i == hdr->e_shstrndx) continue;
		if (shdr[i].sh_type == SHT_STRTAB) {
			strtab = (const char*)(img + shdr[i].sh_offset);
			continue;
		}
		if ((shdr[i].sh_type == SHT_DYNSYM && sym == NULL) ||
		    (shdr[i].sh_type == SHT_SYMTAB) ) {
			sym = (Elf_Sym*)(img + shdr[i].sh_offset);
			sym_count = shdr[i].sh_size / shdr[i].sh_entsize;
			assert (sizeof(Elf_Sym) == shdr[i].sh_entsize);
			is_dyn = (shdr[i].sh_type == SHT_DYNSYM);
		}
	}

	symtab = sym;
	next_sym_idx = 0;

	/* missing some data we expect; don't try to grab any symbols */
	if (symtab == NULL || strtab == NULL)
		sym_count = 0;
}

Symbol* ElfDebug::nextSym(void)
{
	Elf64_Sym	*sym = (Elf64_Sym*)symtab;	/* FIXME */
	Elf64_Sym	*cur_sym;
	std::string	name;

	if (next_sym_idx >= sym_count) return NULL;

	cur_sym = &sym[next_sym_idx++];
	name = std::string(&strtab[cur_sym->st_name]);

	return new Symbol(
		name,
		cur_sym->st_value,
		cur_sym->st_size,
		is_dyn,
		(ELF64_ST_TYPE(cur_sym->st_info) == STT_FUNC));
}
