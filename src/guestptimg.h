/* guest image for a ptrace generated image */
#ifndef GUESTSTATEPTIMG_H
#define GUESTSTATEPTIMG_H

#include <map>
#include "collection.h"
#include "guest.h"
#include "guesttls.h"
extern "C" {
#include <valgrind/libvex_guest_amd64.h>
}

class PTImgMapEntry
{
public:
	PTImgMapEntry(pid_t pid, const char* mapline);
	virtual ~PTImgMapEntry(void);
	unsigned int getByteCount() const
	{
		return ((uintptr_t)mem_end - (uintptr_t)mem_begin);
	}
	void* getBase(void) const { return mem_begin; }
	int getProt(void) const;
	bool isStack(void) const { return is_stack; }
	std::string getLib() const { return libname; }
private:
	void ptraceCopy(pid_t pid, int prot);
	void ptraceCopyRange(pid_t pid, int prot, void* m_beg, void* m_end);
	void mapLib(pid_t pid);
	void mapAnon(pid_t pid);
	void mapStack(pid_t pid);

	void		*mem_begin, *mem_end;
	char		perms[5];
	uint32_t	off;
	int		t[2];
	int		xxx;	/* XXX no idea */
	char		libname[256];

	void		*mmap_base;
	int		mmap_fd;

	bool		is_stack;
};

class PTImgTLS : public GuestTLS
{
public:
	PTImgTLS(void* base) 
	{ 
		delete [] tls_data;
		tls_data = (uint8_t*)base;
		delete_data = false;
	}
	virtual ~PTImgTLS() {}
	virtual unsigned int getSize() const { return 0xd00; }
protected:
};

class GuestPTImg : public Guest
{
public:
	virtual ~GuestPTImg(void) {}
	llvm::Value* addrVal2Host(llvm::Value* addr_v) const { return addr_v; }
	uint64_t addr2Host(guestptr_t guestptr) const { return guestptr; }
	guestptr_t name2guest(const char* symname) const { return 0; }
	void* getEntryPoint(void) const { return entry_pt; }
	
	template <class T>
	static T* create(
		int argc, char* const argv[], char* const envp[])
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
		void* range_begin = 0, void* range_end = 0);


	virtual std::list<GuestMemoryRange*> getMemoryMap(void) const;
	void recordInitialMappings(VexMem&);
protected:
	GuestPTImg(int argc, char* const argv[], char* const envp[]);
	virtual void handleChild(pid_t pid);

	void slurpRegisters(pid_t pid);

	void setBreakpoint(pid_t pid, void* addr);
	void resetBreakpoint(pid_t pid, void* addr);
	void* undoBreakpoint(pid_t pid);
private:

	void dumpSelfMap(void) const;

	pid_t createSlurpedChild(
		int argc, char *const argv[], char *const envp[]);
	void slurpBrains(pid_t pid);
	void slurpMappings(pid_t pid);

	void				*entry_pt;
	PtrList<PTImgMapEntry>		mappings;
	std::map<void*, uint64_t>	breakpoints;
};

#endif
