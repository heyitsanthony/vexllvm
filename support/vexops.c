/* stupid vex count ops */
#include <stdint.h>
#include <valgrind/libvex_ir.h>

/* leading sign bits (not including topmost) */
uint32_t vexop_cls32(uint32_t v)
{
	unsigned ret = 0;
	if (!(v & 0x80000000)) return 0;
	v <<= 1;
	while ((v & 0x80000000)) {
		ret++;
		v <<= 1;
	}
	return ret;
}

/* leading sign bits (not including topmost) */
uint64_t vexop_cls64(uint64_t v)
{
	unsigned ret = 0;
	if (!(v & 0x8000000000000000)) return 0;
	v <<= 1;
	while ((v & 0x8000000000000000)) {
		ret++;
		v <<= 1;
	}
	return ret;
}


/* count trailing zeros-- 11100 = 2, 110 = 1, 11000 = 3 */
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

/* count leading zeros-- 00001 = 4, 01 = 1, 0001, = 3 */
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

double vexop_absf64(double f) { return (f < 0) ? -f : f; }
float vexop_absf32(float f) { return (f < 0)  ? -f : f; }
