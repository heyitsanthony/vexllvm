#include <stdint.h>
#include <valgrind/libvex_ir.h>

extern int32_t float64_is_nan(uint64_t);
extern int32_t float64_eq(uint64_t, uint64_t);
extern int32_t float64_lt(uint64_t, uint64_t);

uint32_t vexop_softfloat_cmpf64(uint64_t f1, uint64_t f2)
{
	/* XXX how does this handle NaN? */
	if (float64_is_nan(f1) || float64_is_nan(f2)) return Ircr_UN;
	if (float64_eq(f1, f2)) return Ircr_EQ;
	if (float64_lt(f1, f2)) return Ircr_LT;
	return Ircr_GT;
}