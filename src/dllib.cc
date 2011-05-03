#include <dlfcn.h>
#include <stdio.h>
#include <assert.h>
#include "dllib.h"

DLLib* DLLib::load(const char* libname)
{
	DLLib* lib = new DLLib(libname);

	if (!lib->dl_h)  {
		delete lib;
		return 0;
	}

	return lib;
}

DLLib::DLLib(const char* in_libname)
: libname(in_libname)
{
	dl_h = dlopen(
		libname, 
		RTLD_NOW | RTLD_LOCAL
		/* or can we hook into the lazy loader? */);
}

DLLib::~DLLib(void)
{
	if (dl_h) dlclose(dl_h);
}

void* DLLib::resolve(const char* symname)
{
	const char*	err;
	void*		ret;

	dlerror();	/* clear errors */
	ret = dlsym(dl_h, symname);
	err = dlerror();
	if (err) fprintf(stderr, "dlsym OOPS: %s\n", err);

	return ret;
}
