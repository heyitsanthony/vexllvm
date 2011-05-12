#include <stdint.h>
#include <string.h>
#include <assert.h>

int main(int argc, char* argv[])
{
	uint8_t	x[37];
	int	i;
	memset(x, 0xa1, sizeof(x));
	for (i = 0; i < sizeof(x); i++) {
		assert (x[i] == 0xa1);
	}
	return 0;
}