#include <stdio.h>
#include <assert.h>

int main(int argc, char* argv[])
{
	int	i;
	for (i = 0; i < argc; i++) {
		assert (argv[i] != NULL);
		printf("argv[%d]: %s\n", i, argv[i]);
	}
	return argc;
}