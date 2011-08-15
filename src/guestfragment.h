/* a guest for representing a single code fragment */
#ifndef VEXLLVM_GUESTFRAGMENT_H
#define VEXLLVM_GUESTFRAGMENT_H

#include "guest.h"

class GuestFragment : public Guest
{
public:
	GuestFragment(
		Arch::Arch,
		const char* data,
		guest_ptr base,
		unsigned int len);
	virtual ~GuestFragment();
	virtual guest_ptr getEntryPoint(void) const { return guest_base; }
	virtual std::string getName(guest_ptr) const;
	virtual const Symbols* getSymbols(void) const { return NULL; }
	virtual Arch::Arch getArch() const { return arch; }
	unsigned int getCodeLength(void) const { return code_len; }
	unsigned int getCodePageBytes(void) const
	{ return 4096*((code_len + 4095) / 4096); }

	static GuestFragment* fromFile(
		const char* fname,
		Arch::Arch arch,
		guest_ptr base);
private:
	Arch::Arch	arch;	/* seriously? */
	guest_ptr	guest_base;
	char		*code;
	unsigned int	code_len;
};

#endif
