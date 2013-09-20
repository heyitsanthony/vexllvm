#ifndef I386_MACROS_H
#define I386_MACROS_H

#define ud2vexseg(y,x) do {					\
	(x)->Bits.LimitLow = (y).limit & 0xffff;		\
	(x)->Bits.BaseLow = (y).base_addr & 0xffff;		\
	(x)->Bits.BaseMid = ((y).base_addr >> 16) & 0xff;	\
	(x)->Bits.Type = 0; /* ??? */				\
	(x)->Bits.Dpl = 3;					\
	(x)->Bits.Pres = !(y).seg_not_present;			\
	(x)->Bits.LimitHi = (y).limit >> 16;			\
	(x)->Bits.Sys = 0;					\
	(x)->Bits.Reserved_0 = 0;				\
	(x)->Bits.Default_Big = 1;				\
	(x)->Bits.Granularity = (y).limit_in_pages;		\
	(x)->Bits.BaseHi = ((y).base_addr >> 24) & 0xff;	\
} while (0)

#endif
