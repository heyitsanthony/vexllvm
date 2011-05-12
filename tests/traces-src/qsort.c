#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#define NUM_MEMBERS	10

static int intcmp(const void* v1, const void* v2)
{
	return *((int32_t*)v1)  - *((int32_t*)v2);
}

static int32_t sort_array[NUM_MEMBERS];

int main(int argc, char* argv[])
{
	int	i;
	srand(1234);
	for (i = 0; i < NUM_MEMBERS; i++) sort_array[i] = i;
	for (i = 0; i < NUM_MEMBERS; i++) {
		int	r = rand() % NUM_MEMBERS;
		int32_t	tmp;
		tmp = sort_array[r];
		sort_array[r] = sort_array[i];
		sort_array[i] = tmp;
	}
	qsort(&sort_array, NUM_MEMBERS, sizeof(int32_t), intcmp);

	for (i = 1; i < NUM_MEMBERS; i++) {
		assert (sort_array[i-1] < sort_array[i] && "NOT SORTED");
	}

	return 0;
}
