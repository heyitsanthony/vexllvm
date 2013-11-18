/* guest mem interface for ptraced process */
#ifndef GUESTMEMDUAL_H
#define GUESTMEMDUAL_H

#include <stdint.h>
#include "guestptimg.h"
#include "guestmem.h"

class GuestMemDual : public GuestMem
{
public:
	static GuestMemDual* createImported(GuestMem* gm0, GuestMem* gm1)
	{
		GuestMemDual* gmd = new GuestMemDual(gm0, gm1);
		gmd->maps = gm0->getMapMap();
		return gmd;
	}
	GuestMemDual(GuestMem* gm0, GuestMem* gm1);
	virtual ~GuestMemDual(void);

	#define DEFREAD(x)	\
	virtual uint##x##_t read##x(guest_ptr offset) const;
	DEFREAD(8)
	DEFREAD(16)
	DEFREAD(32)
	DEFREAD(64)
	#undef DEFREAD

	#define DEFWRITE(x)	\
	virtual void write##x(guest_ptr offset, uint##x##_t t);
	DEFWRITE(8)
	DEFWRITE(16)
	DEFWRITE(32)
	DEFWRITE(64)
	#undef DEFWRITE

	virtual void* getData(const Mapping& m) const;

	virtual void memcpy(guest_ptr dest, const void* src, size_t len);
	virtual void memcpy(void* dest, guest_ptr src, size_t len) const;
	virtual void memset(guest_ptr dest, char d, size_t len);
	virtual int strlen(guest_ptr p) const;
	virtual bool sbrk(guest_ptr new_top);
	virtual int mmap(guest_ptr& result, guest_ptr addr, size_t length,
	 	int prot, int flags, int fd, off_t offset);
	virtual int mprotect(guest_ptr offset, size_t length, int prot);
	virtual int munmap(guest_ptr offset, size_t length);
	virtual int mremap(
		guest_ptr& result, guest_ptr old_offset,
		size_t old_length, size_t new_length,
		int flags, guest_ptr new_offset);
private:
	GuestMem	*m[2];	/* not owner, do not delete */
};

#endif
