#include <assert.h>
#include <stdint.h>
#include <string.h>

#define MEMSET_BYTES	8192

uint8_t	memset_data[MEMSET_BYTES];

int main(int argc, char* argv[])
{
	int	i;

	memset(memset_data, 0xa1, MEMSET_BYTES);
	for (i = 0; i < MEMSET_BYTES; i++) {
		assert (memset_data[i] == 0xa1 && "WRONG MEMSET VALUE");
	}

	return 0;
}