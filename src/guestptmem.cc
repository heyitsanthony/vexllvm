#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include "guestptmem.h"
#include "ptimgarch.h"

GuestPTMem::GuestPTMem(GuestPTImg* gpimg, pid_t in_pid)
: g_ptimg(gpimg)
, pid(in_pid)
{ /* should I bother with tracking the memory maps?*/ }


GuestPTMem::~GuestPTMem(void) { }


#define DEFREAD(x)	\
uint##x##_t GuestPTMem::read##x(guest_ptr offset) const	\
{	\
	uint64_t	n;	\
	n = ptrace(PTRACE_PEEKDATA, pid, (void*)offset.o, NULL);	\
	return (uint##x##_t)n;	}
DEFREAD(8)
DEFREAD(16)
DEFREAD(32)
DEFREAD(64)
#undef DEFREAD

#define DEFWRITE(x)	\
void GuestPTMem::write##x(guest_ptr offset, uint##x##_t t)	\
{	uint64_t	n;						\
	n = ptrace(PTRACE_PEEKDATA, pid, (void*)offset.o, NULL);	\
	n &= ~(uint64_t)(((uint##x##_t)(t | ~t)));			\
	n |= t;								\
	ptrace(PTRACE_POKEDATA, pid, (void*)offset.o, (void*)n); }
DEFWRITE(8)
DEFWRITE(16)
DEFWRITE(32)
DEFWRITE(64)
#undef DEFWRITE


void GuestPTMem::memcpy(guest_ptr dest, const void* src, size_t len)
{ g_ptimg->getPTArch()->copyIn(dest, src, len); }

void GuestPTMem::memcpy(void* dest, guest_ptr src, size_t len) const
{
	char	*v = (char*)dest;
	for (unsigned i = 0; i < len; i++)
		v[i] = read8(src + i);
}

void GuestPTMem::memset(guest_ptr dest, char d, size_t len)
{
	for (unsigned i = 0; i < len; i++)
		write8(dest + i, d);
}

int GuestPTMem::strlen(guest_ptr p) const
{
	int	n = 0;
	while (read8(p)) {
		p.o++;
		n++;
	}
	return n;
}

bool GuestPTMem::sbrk(guest_ptr new_top) { assert (0 == 1 && "STUB"); }

int GuestPTMem::mmap(
	guest_ptr& result, guest_ptr addr, size_t length,
	int prot, int flags, int fd, off_t offset)
{
	SyscallParams	sp(SYS_mmap, addr.o, length, prot, flags, fd, offset);
	result.o = g_ptimg->getPTArch()->dispatchSysCall(sp);
	return ((void*)result.o == MAP_FAILED) ? -1 : 0;
}

int GuestPTMem::mprotect(guest_ptr offset, size_t length, int prot)
{
	SyscallParams	sp(SYS_mprotect, offset.o, length, prot, 0, 0, 0);
	g_ptimg->getPTArch()->dispatchSysCall(sp);
	return 0;
}

int GuestPTMem::munmap(guest_ptr offset, size_t length)
{
	SyscallParams	sp(SYS_munmap, offset.o, length, 0, 0, 0, 0);
	g_ptimg->getPTArch()->dispatchSysCall(sp);
	return 0;
}

int GuestPTMem::mremap(
	guest_ptr& result, guest_ptr old_offset,
	size_t old_length, size_t new_length,
	int flags, guest_ptr new_offset)
{
	assert (0 == 1 && "STUB");
	return 0;
}
