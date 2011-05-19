#include <stdio.h>
#include <dlfcn.h>
#include <assert.h>

int main(int argc, char* argv[])
{
	void	*h, *sym;
	h = dlopen("/lib/libc.so.6", RTLD_LOCAL | RTLD_LAZY);
	assert (h != NULL);
	sym = dlsym(h, "printf");
	assert (sym != NULL);
	printf("printf=%p\n", sym);
	dlclose(h);
	return 0;
}