#ifndef GUESTSNAPSHOT_H
#define GUESTSNAPSHOT_H

#include <list>
#include "guest.h"

class GuestSnapshot : public Guest
{
public:
	static GuestSnapshot* create(const char* dirname);
	virtual ~GuestSnapshot(void);
	static void save(const Guest*, const char* dirname);
	virtual guest_ptr getEntryPoint(void) const { return entry_pt; }
	virtual Arch::Arch getArch(void) const { return arch; }

	virtual const Symbols* getSymbols(void) const { return syms; }
protected:
	GuestSnapshot(const char* dirname);

private:
	static void saveMappings(const Guest* g, const char* dirpath);
	static void saveSymbols(const Guest* g, const char* dirpath);
	void loadSymbols(const char* dirname);
	void loadMappings(const char* dirname);

	bool		is_valid;
	guest_ptr	entry_pt;
	Arch::Arch	arch;
	std::list<int>	fd_list;
	Symbols		*syms;
};

#endif
