#include "guestmemdual.h"

GuestMemDual::GuestMemDual(GuestMem* gm0, GuestMem* gm1)
{
	m[0] = gm0;
	m[1] = gm1;
}

GuestMemDual::~GuestMemDual(void) {}

#define DEFREAD(x)	\
uint##x##_t GuestMemDual::read##x(guest_ptr offset) const	\
{ return m[0]->read##x(offset); }
DEFREAD(8)
DEFREAD(16)
DEFREAD(32)
DEFREAD(64)
#undef DEFREAD

#define DEFWRITE(x)	\
void GuestMemDual::write##x(guest_ptr offset, uint##x##_t t)	\
{ m[0]->write##x(offset,t); m[1]->write##x(offset,t); }
DEFWRITE(8)
DEFWRITE(16)
DEFWRITE(32)
DEFWRITE(64)
#undef DEFWRITE

void* GuestMemDual::getData(const Mapping& _m) const
{ return m[0]->getData(_m); }

void GuestMemDual::memcpy(guest_ptr dest, const void* src, size_t len)
{
	m[0]->memcpy(dest, src, len);
	m[1]->memcpy(dest, src, len);
}

void GuestMemDual::memcpy(void* dest, guest_ptr src, size_t len) const
{ m[0]->memcpy(dest, src, len); }

void GuestMemDual::memset(guest_ptr dest, char d, size_t len)
{
	m[0]->memset(dest, d, len);
	m[1]->memset(dest, d, len);
}

int GuestMemDual::strlen(guest_ptr p) const { return m[0]->strlen(p); }

bool GuestMemDual::sbrk(guest_ptr new_top)
{ return m[0]->sbrk(new_top) | m[1]->sbrk(new_top); }

int GuestMemDual::mmap(guest_ptr& result, guest_ptr addr, size_t length,
	int prot, int flags, int fd, off_t offset)
{
	return	m[0]->mmap(result, addr, length, prot, flags, fd, offset) |
		m[1]->mmap(result, addr, length, prot, flags, fd, offset);
}

int GuestMemDual::mprotect(guest_ptr offset, size_t length, int prot)
{
	return	m[0]->mprotect(offset, length, prot) |
		m[1]->mprotect(offset, length, prot);
}

int GuestMemDual::munmap(guest_ptr offset, size_t length)
{ return m[0]->munmap(offset, length) | m[1]->munmap(offset, length); }

int GuestMemDual::mremap(
	guest_ptr& result, guest_ptr old_offset,
	size_t old_length, size_t new_length,
	int flags, guest_ptr new_offset)
{
	return	m[0]->mremap(
			result, old_offset, old_length,
			new_length, flags, new_offset)	|
		m[1]->mremap(
			result, old_offset, old_length,
			new_length, flags, new_offset);
}
