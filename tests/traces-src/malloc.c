#include <stdlib.h>
#include <assert.h>
#include <string.h>

int main(int argc, char* argv[])
{
	void	*m = malloc(100);
	assert (m != NULL);
	memset(m, 0, 100);

	return 0;
}