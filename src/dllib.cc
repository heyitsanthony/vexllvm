#include <dlfcn.h>
#include <stdio.h>
#include <assert.h>
#include "dllib.h"

Lmid_t	DLLib::lmid = 0;

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
	if (lmid == 0) {
		/* first time loading-- establish our very own 
		 * namespace. */
		dl_h = dlmopen(
			LM_ID_NEWLM,
			libname,
			RTLD_NOW | RTLD_DEEPBIND |  RTLD_LOCAL);
		assert (dl_h != NULL && "Failed to open lib with new lmid");
		dlinfo(dl_h, RTLD_DI_LMID, &lmid);
		assert (lmid != NULL);
		return;
	}

	dl_h = dlmopen(
		lmid,
		libname, 
		RTLD_NOW | RTLD_DEEPBIND |  RTLD_LOCAL
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
