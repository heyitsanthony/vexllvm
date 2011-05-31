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

// Too lazy to write my own. Taken from:
// http://www.azillionmonkeys.com/qed/sqroot.html
double vexop_sqrtf64(double y)
{
	double x, z, tempf;
	unsigned long *tfptr = ((unsigned long *)&tempf) + 1;

	tempf = y;
	*tfptr = (0xbfcdd90a - *tfptr)>>1; /* estimate of 1/sqrt(y) */
	x =  tempf;
	z =  y*0.5;                        /* hoist out the "/2"    */
	x = (1.5*x) - (x*x)*(x*z);         /* iteration formula     */
	x = (1.5*x) - (x*x)*(x*z);
	x = (1.5*x) - (x*x)*(x*z);
	x = (1.5*x) - (x*x)*(x*z);
	x = (1.5*x) - (x*x)*(x*z);

	return x*y;
}