/* guest image for a ptrace generated image */
#ifndef GUESTSTATEPTIMG_H
#define GUESTSTATEPTIMG_H

#include "collection.h"
#include "gueststate.h"
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
private:
	void ptraceCopy(pid_t pid, int prot);
	void mapLib(pid_t pid);
	void mapAnon(pid_t pid);
	int getProt(void) const;

	void		*mem_begin, *mem_end;
	char		perms[5];
	uint32_t	off;
	int		t[2];
	int		xxx;	/* XXX no idea */
	char		libname[256];

	void		*mmap_base;
	int		mmap_fd;
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

class GuestStatePTImg : public GuestState
{
public:
	GuestStatePTImg(int argc, char* const argv[], char* const envp[]);
	virtual ~GuestStatePTImg(void) {}
	llvm::Value* addrVal2Host(llvm::Value* addr_v) const { return addr_v; }
	uint64_t addr2Host(guestptr_t guestptr) const { return guestptr; }
	guestptr_t name2guest(const char* symname) const { return 0; }
	void* getEntryPoint(void) const { return entry_pt; }
	
	bool continueWithBounds(uint64_t start, uint64_t end, const VexGuestAMD64State& state);
	void printSubservient(std::ostream& os, const VexGuestAMD64State* ref = 0);
	void stackTraceSubservient(std::ostream& os);
	void printTraceStats(std::ostream& os);
private:
	void dumpSelfMap(void) const;
	void			slurpBrains(pid_t pid);
	void			slurpMappings(pid_t pid);
	void			slurpRegisters(pid_t pid);

	void			*entry_pt;
	PtrList<PTImgMapEntry>	mappings;
	pid_t			child_pid;
	std::string		binary;
	uint64_t		steps;
	uint64_t		blocks;
};

#endif
