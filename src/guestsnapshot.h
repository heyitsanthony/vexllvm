#ifndef GUESTSNAPSHOT_H
#define GUESTSNAPSHOT_H

#include <list>
#include "guest.h"

class GuestSnapshot : public Guest
{
public:
	static GuestSnapshot* create(GuestMem* mem, const char* dirname);
	virtual ~GuestSnapshot(void);
	static void save(const Guest*, const char* dirname);
	virtual void* getEntryPoint(void) const { return entry_pt; }
	virtual Arch::Arch getArch(void) const { return arch; }

protected:
	GuestSnapshot(GuestMem* mem, const char* dirname);

private:
	bool		is_valid;
	void		*entry_pt;
	Arch::Arch	arch;
	std::list<int>	fd_list;
};

#endif
