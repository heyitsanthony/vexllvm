#ifndef GUESTSNAPSHOT_H
#define GUESTSNAPSHOT_H

#include <list>
#include "guest.h"

class GuestSnapshot : public Guest
{
public:
	static GuestSnapshot* create(const char* dirname);
	static char* readMemory(const char* dirname, guest_ptr p, unsigned int len);
	virtual ~GuestSnapshot(void);
	static void save(const Guest*, const char* dirname);
	virtual guest_ptr getEntryPoint(void) const { return entry_pt; }
	virtual Arch::Arch getArch(void) const { return arch; }

	virtual const Symbols* getSymbols(void) const { return syms; }
	virtual Symbols* getSymbols(void) { return syms; }
	virtual const Symbols* getDynSymbols(void) const { return dyn_syms; }
	virtual std::vector<guest_ptr> getArgvPtrs(void) const
	{ return argv_ptrs; }

	const std::string& getPath(void) const { return srcdir; }

	void getPlatform(const char* plat_key, void* buf, unsigned len) const;
protected:
	GuestSnapshot(const char* dirname);

private:
	static void saveMappings(const Guest* g, const char* dirpath);
	static void saveSymbols(
		const Symbols* g_syms,
		const char* dirpath,
		const char* name);

	Symbols* loadSymbols(const char* name);
	void loadMappings(void);

	bool			is_valid;
	guest_ptr		entry_pt;
	Arch::Arch		arch;
	std::list<int>		fd_list;
	Symbols			*syms;
	Symbols			*dyn_syms;
	std::vector<guest_ptr>	argv_ptrs;

	std::string		srcdir;
};

#endif
