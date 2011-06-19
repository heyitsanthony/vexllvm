/* stupid vex count ops */
#include <stdint.h>
#include <valgrind/libvex_ir.h>

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

uint32_t vexop_ctz32(uint32_t v)
{
	int	ret = 0;
	if (v == 0) return 32;
	while ((v & 1) == 0) {
		ret++;
		v >>= 1;
	}
	return ret;
}

uint32_t vexop_clz32(uint32_t v)
{
	int	ret = 0;
	if (v == 0) return 32;
	while ((v & (1UL << 31UL)) == 0) {
		ret++;
		v <<= 1;
	}
	return ret;
}

uint32_t vexop_cmpf64(double f1, double f2)
{
	if (f1 == f2) return Ircr_EQ;
	if (f1 < f2) return Ircr_LT;
	if (f1 > f2) return Ircr_GT;
	return Ircr_UN;
}

uint32_t vexop_cmpf32(float f1, float f2)
{
	if (f1 == f2) return Ircr_EQ;
	if (f1 < f2) return Ircr_LT;
	if (f1 > f2) return Ircr_GT;
	return Ircr_UN;
}

double vexop_minf64(double f1, double f2)
{
	if (f1 < f2) return f1;
	return f2;
}

float vexop_maxf32(float f1, float f2)
{
	if (f1 > f2) return f1;
	return f2;
}
