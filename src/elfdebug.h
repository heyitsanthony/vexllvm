#ifndef ELFDEBUG_H
#define ELFDEBUG_H

class Symbol;
class Symbols;
class GuestMem;

/* stupid little class to slurp up symbols */
/* we need to find out who debugging information really well / robustly
 * and integrate it with us */
class ElfDebug
{
public:
	static Symbols* getSyms(const char* elf_path, 
		uintptr_t base = 0);

	static Symbols* getLinkageSyms(
		const GuestMem* m, const char* elf_path);

private:
	ElfDebug(const char* path);
	virtual ~ElfDebug(void);
	template <
		typename Elf_Ehdr, typename Elf_Shdr,
		typename Elf_Sym>
		void setupTables(void);

	Symbol	*nextSym(void);
	Symbol	*nextLinkageSym(const GuestMem* m);

	bool	is_valid;

	int		fd;
	char		*img;
	unsigned int	img_byte_c;

	void		*symtab;
	unsigned int	next_sym_idx;
	unsigned int	sym_count;
	const char	*strtab;
	bool		is_dyn;

	void		*rela_tab;
	unsigned int	next_rela_idx;
	unsigned int	rela_count;
	void		*dynsymtab;
	unsigned int	dynsym_count;
	const char	*dynstrtab;

};

#endif
