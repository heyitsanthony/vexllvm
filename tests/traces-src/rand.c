#include <assert.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
	int	i;
	srand(1234);
	for (i = 0; i < 100; i++) {
		int	r;
		r = rand() % 10;
		assert (r < 10);
	}
	return 0;
}