/* stupid vex count ops */
#include <stdint.h>

uint64_t vexop_ctz64(uint64_t v)
{
	int	ret = 0;
	if (v == 0) return 64;
	while ((v & 1) == 0) {
		ret++;
		v >>= 1;
	}
	return ret;
}

uint64_t vexop_clz64(uint64_t v)
{
	int	ret = 0;
	if (v == 0) return 64;
	while ((v & (1ULL << 63ULL)) == 0) {
		ret++;
		v <<= 1;
	}
	return ret;
}