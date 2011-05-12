#include <stdlib.h>
#include <assert.h>

int main(int argc, char* argv[])
{
	void	*m = malloc(100);
	assert (m != NULL);
	return 0;
}