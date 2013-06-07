/* write-only guest memory */
#ifndef GUESTMEMSINK_H
#define GUESTMEMSINK_H

#include <stdlib.h>
#include "guestmem.h"

class GuestMemSink : public GuestMem
{
public:
	GuestMemSink(void);
	virtual ~GuestMemSink(void);

	#define DEFREAD(x)	\
	virtual uint##x##_t read##x(guest_ptr offset) const { abort(); }
	DEFREAD(8)
	DEFREAD(16)
	DEFREAD(32)
	DEFREAD(64)
	#undef DEFREAD

	#define DEFWRITE(x)	\
	virtual void write##x(guest_ptr offset, uint##x##_t t) { /* ignore */ }
	DEFWRITE(8)
	DEFWRITE(16)
	DEFWRITE(32)
	DEFWRITE(64)
	#undef DEFWRITE

	virtual void memcpy(guest_ptr dest, const void* src, size_t len)
	{ /* ignore */ }
	virtual void memcpy(void* dest, guest_ptr src, size_t len) const
	{ abort(); }
	virtual void memset(guest_ptr dest, char d, size_t len)
	{ abort(); }
	
	virtual int strlen(guest_ptr p) const { abort(); }
protected:
	virtual void* sys_mmap(void*, size_t len, int prot, int fl,
		int fd, off_t off) const;
	virtual int sys_munmap(void* p, unsigned len) const;
	virtual void* sys_mremap(
		void* old, size_t oldsz, size_t newsz,
		int fl, void* new_addr = NULL) const;
	virtual int sys_mprotect(void*, size_t len, int prot) const;
};

#endif
