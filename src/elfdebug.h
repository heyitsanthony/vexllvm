#ifndef ELFDEBUG_H
#define ELFDEBUG_H

class Symbol;
class Symbols;

/* stupid little class to slurp up symbols */
/* we need to find out who debugging information really well / robustly
 * and integrate it with us */
class ElfDebug
{
public:
	static Symbols* getSyms(const char* elf_path, void* base = NULL);

private:
	ElfDebug(const char* path);
	virtual ~ElfDebug(void);
	template <typename Elf_Ehdr, typename Elf_Shdr, typename Elf_Sym>
		void setupTables(void);

	Symbol	*nextSym(void);
	bool	is_valid;

	int		fd;
	char		*img;
	unsigned int	img_byte_c;

	void		*symtab;
	unsigned int	next_sym_idx;
	unsigned int	sym_count;
	const char	*strtab;
	bool		is_dyn;
};

#endif
