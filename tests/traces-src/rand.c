#include <assert.h>
#include <stdlib.h>

#define NUM_LOOPS	3

int main(int argc, char* argv[])
{
	int	i;
	srand(1234);
	for (i = 0; i < NUM_LOOPS; i++) {
		int	r;
		r = rand() % 10;
		assert (r < 10);
	}
	return 0;
}