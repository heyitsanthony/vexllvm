/* guest image for a ptrace generated image */
#ifndef GUESTSTATEPTIMG_H
#define GUESTSTATEPTIMG_H

#include <map>
#include "collection.h"
#include "guest.h"
extern "C" {
#include <valgrind/libvex_guest_amd64.h>
}

class Symbols;

class PTImgMapEntry
{
public:
	PTImgMapEntry(GuestMem* mem, pid_t pid, const char* mapline);
	virtual ~PTImgMapEntry(void);
	unsigned int getByteCount() const
	{
		return ((uintptr_t)mem_end - (uintptr_t)mem_begin);
	}
	guest_ptr getBase(void) const { return mem_begin; }
	guest_ptr getEnd(void) const { return getBase() + getByteCount(); }
	int getProt(void) const;
	std::string getLib() const { return libname; }
private:
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
};

class GuestPTImg : public Guest
{
public:
	virtual ~GuestPTImg(void);
	guest_ptr getEntryPoint(void) const { return entry_pt; }

	template <class T>
	static T* create(int argc, char* const argv[], char* const envp[])
	{
		GuestPTImg		*pt_img;
		T			*pt_t;
		pid_t			slurped_pid;

		pt_t = new T(argc, argv, envp);
		pt_img = pt_t;
		slurped_pid = pt_img->createSlurpedChild(argc, argv, envp);
		if (slurped_pid <= 0) {
			delete pt_img;
			return NULL;
		}

		pt_img->handleChild(slurped_pid);
		return pt_t;
	}

	void printTraceStats(std::ostream& os);
	static void stackTrace(
		std::ostream& os, const char* binname, pid_t pid,
		guest_ptr range_begin = guest_ptr(0), 
		guest_ptr range_end = guest_ptr(0));

	static void dumpSelfMap(void);
	virtual Arch::Arch getArch() const;

	virtual const Symbols* getSymbols(void) const;
	virtual std::vector<guest_ptr> getArgvPtrs(void) const
	{ return argv_ptrs; }

protected:
	GuestPTImg(int argc, char* const argv[], char* const envp[]);
	virtual void handleChild(pid_t pid);

	void slurpRegisters(pid_t pid);

	void setBreakpoint(pid_t pid, guest_ptr addr);
	void resetBreakpoint(pid_t pid, guest_ptr addr);
	guest_ptr undoBreakpoint(pid_t pid);
private:
	pid_t createSlurpedChild(
		int argc, char *const argv[], char *const envp[]);
	void slurpBrains(pid_t pid);
	void slurpMappings(pid_t pid);
	void loadSymbols(void) const;

	guest_ptr			entry_pt;
	PtrList<PTImgMapEntry>		mappings;
	std::map<guest_ptr, uint64_t>	breakpoints;
	mutable Symbols			*symbols; // lazy loaded
	std::vector<guest_ptr>		argv_ptrs;
};

#endif
