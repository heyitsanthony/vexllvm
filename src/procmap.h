#ifndef PROCMAP_H
#define PROCMAP_H

#include "guestptr.h"
#include "guestmem.h"

class ProcMap
{
public:
	virtual ~ProcMap(void);
	unsigned int getByteCount() const
	{ return ((uintptr_t)mem_end - (uintptr_t)mem_begin); }
	guest_ptr getBase(void) const { return mem_begin; }
	guest_ptr getEnd(void) const { return getBase() + getByteCount(); }
	int getProt(void) const;
	std::string getLib() const { return libname; }

	static void slurpMappings(
		pid_t pid,
		GuestMem* m,
		PtrList<ProcMap>& ents,
		bool do_copy = true);

	static ProcMap* create(
		GuestMem* mem, pid_t pid, const char* mapline, bool copy=true);

	static bool dump_maps;

protected:
	ProcMap(GuestMem* mem, pid_t pid, const char* mapline, bool copy=true);
private:
	void copyRange(pid_t pid, guest_ptr m_beg, guest_ptr m_end);
	bool procMemCopy(pid_t pid, guest_ptr m_beg, guest_ptr m_end);
	void ptraceCopy(pid_t pid, int prot);
	void ptraceCopyRange(pid_t pid, guest_ptr m_beg, guest_ptr m_end);
	void mapLib(pid_t pid);
	void mapAnon(pid_t pid);
	void mapStack(pid_t pid);

	guest_ptr	mem_begin, mem_end;
	char		perms[5];
	uint32_t	off;
	int		t[2];
	int		xxx;	/* XXX no idea */
	char		libname[256];

	guest_ptr	mmap_base;
	int		mmap_fd;

	GuestMem	*mem;

	bool		copy;	/* whether to copy from ptraced proc to 'mem' */
};

#endif
