#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
int main(int argc, char* argv[])
{
	const char	*u;
	u = getenv("USER");
	assert (u != NULL);
	printf("%s\n", u);
	return 0;
}