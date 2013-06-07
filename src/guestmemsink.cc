#include "guestmemsink.h"
#include "Sugar.h"

GuestMemSink::GuestMemSink(void)
{}

void* GuestMemSink::sys_mmap(
	void* p, size_t len, int prot, int fl, int fd, off_t off) const
{
	if (p != NULL) return p;
	assert (0 ==1 && "fix alloc len");
	abort();
}

int GuestMemSink::sys_munmap(void* p, unsigned len) const
{ return 0; }

int GuestMemSink::sys_mprotect(void* p, size_t len, int prot) const
{ return 0; }

void* GuestMemSink::sys_mremap(
	void* old, size_t oldsz, size_t newsz, int fl, void* new_addr) const
{
	if (new_addr != NULL) return new_addr;
	assert(0 ==1  && "uhhh");
	abort();
}

GuestMemSink::~GuestMemSink(void)
{
	foreach (it, maps.begin(), maps.end())
		delete it->second;
	maps.clear();
}
