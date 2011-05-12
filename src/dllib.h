#ifndef DLLIB_H
#define DLLIB_H

#include <dlfcn.h>

class DLLib
{
public:
	virtual ~DLLib(void);
	static DLLib* load(const char* libname);
	void* resolve(const char* symname);
	const char* getName(void) const { return libname; }
protected:
	DLLib(const char* libname);
private:
	static Lmid_t	lmid;
	const char	*libname;
	void		*dl_h;	
};

#endif
