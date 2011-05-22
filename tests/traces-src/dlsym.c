#include <stdio.h>
#include <dlfcn.h>
#include <assert.h>

int main(int argc, char* argv[])
{
	void	*h, *sym;
	void (*puts_f)(const char*);

	h = dlopen("/lib/libc.so.6", RTLD_LOCAL | RTLD_LAZY);
	assert (h != NULL);
	sym = dlsym(h, "puts");
	assert (sym != NULL);
	puts_f = sym;
	puts_f("hello\n");
	dlclose(h);
	return 0;
}