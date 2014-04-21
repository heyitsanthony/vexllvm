/* guest image for a ptrace generated image */
#ifndef GUESTSTATEPTIMG_H
#define GUESTSTATEPTIMG_H

#include <map>
#include <set>
#include <stdlib.h>
#include "collection.h"
#include "guest.h"
#include "procmap.h"

class Symbols;
class PTImgArch;

#if defined(__amd64__)
#define NEW_ARCH_PT	\
	(arch != Arch::I386)	\
	? (PTImgArch*)(new PTImgAMD64(this, pid))	\
	: (PTImgArch*)(new PTImgI386(this, pid))
#elif defined(__arm__)
#define NEW_ARCH_PT	new PTImgARM(this, pid);
#else
#define NEW_ARCH_PT	0; assert (0 == 1 && "UNKNOWN PTRACE HOST ARCHITECTURE! AIEE");
#endif

class GuestPTImg : public Guest
{
public:
	/* fixup type specifies the state that will get clobbered */
	enum FixupDir { FIXUP_NATIVE = -1, FIXUP_NONE = 0, FIXUP_GUEST = 1 };

	virtual ~GuestPTImg(void);
	guest_ptr getEntryPoint(void) const { return entry_pt; }

	template <class T>
	static T* create(int argc, char* const argv[], char* const envp[])
	{
		GuestPTImg		*pt_img;
		T			*pt_t;
		pid_t			slurped_pid;
		int			sys_nr;
		const char		*bp;

		bp = (getenv("VEXLLVM_REAL_BINPATH"))
			? getenv("VEXLLVM_REAL_BINPATH")
			: argv[0];
		pt_t = new T(bp);
		pt_img = pt_t;
		
		sys_nr = (getenv("VEXLLVM_WAIT_SYSNR") != NULL)
			? atoi(getenv("VEXLLVM_WAIT_SYSNR"))
			: -1;

		slurped_pid = (sys_nr == -1)
			? pt_img->createSlurpedChild(argc, argv, envp)
			: pt_img->createSlurpedOnSyscall(
				argc, argv, envp, sys_nr);

		if (slurped_pid <= 0) {
			delete pt_img;
			return NULL;
		}

		pt_img->handleChild(slurped_pid);
		return pt_t;
	}

	template <class T>
	static T* createAttached(
		int pid,
		int argc, char* const argv[], char* const envp[])
	{
		GuestPTImg		*pt_img;
		T			*pt_t;
		pid_t			slurped_pid;

		pt_t = new T(argv[0], false /* ignore binary */);
		pt_img = pt_t;

		slurped_pid = pt_img->createSlurpedAttach(pid);
		if (slurped_pid <= 0) {
			delete pt_img;
			return NULL;
		}

		pt_img->handleChild(slurped_pid);
		return pt_t;
	}

	/* NOTE: destroys the guest 'gs' on success */
	template <class T>
	static T* create(Guest* gs)
	{
		GuestPTImg		*pt_img;
		T			*pt_t;
		pid_t			slurped_pid;

		pt_t = new T(gs->getBinaryPath(), false /* ignore binary */);
		pt_img = pt_t;
		slurped_pid = pt_img->createFromGuest(gs);
		if (slurped_pid <= 0) {
			delete pt_img;
			return NULL;
		}

		pt_img->handleChild(slurped_pid);
		return pt_t;
	}


	void printTraceStats(std::ostream& os);

	virtual Arch::Arch getArch() const { return arch; }
	virtual const Symbols* getSymbols(void) const;
	virtual Symbols* getSymbols(void);
	virtual const Symbols* getDynSymbols(void) const;

	virtual std::vector<guest_ptr> getArgvPtrs(void) const
	{ return argv_ptrs; }

	virtual guest_ptr getArgcPtr(void) const { return argc_ptr; }


	void setBreakpointByPID(pid_t pid, guest_ptr addr);
	void resetBreakpointByPID(pid_t pid, guest_ptr addr);

	static Symbols* loadSymbols(const PtrList<ProcMap>& mappings);
	static Symbols* loadDynSymbols(
		GuestMem	*mem,
		const char	*binpath);

	static void stackTrace(
		std::ostream& os, const char* binname, pid_t pid,
		guest_ptr range_begin = guest_ptr(0), 
		guest_ptr range_end = guest_ptr(0));

	static void dumpSelfMap(void);

	PTImgArch* getPTArch(void) const { return pt_arch; }
	guest_ptr undoBreakpoint(pid_t pid);

	void slurpRegisters(pid_t pid);
protected:
	GuestPTImg(const char* binpath, bool use_entry=true);
	virtual void handleChild(pid_t pid);

	virtual void slurpBrains(pid_t pid);
	virtual pid_t createSlurpedAttach(int pid);
	void attachSyscall(int pid);
	void fixupRegsPreSyscall(int pid);
	void slurpThreads(void);

	PTImgArch		*pt_arch;
	Arch::Arch		arch;
	PtrList<ProcMap>	mappings;
	guest_ptr		entry_pt;
private:
	pid_t createSlurpedChild(
		int argc, char *const argv[], char *const envp[]);
	pid_t createFromGuest(Guest* gs);
	pid_t createSlurpedOnSyscall(
		int argc, char *const argv[], char *const envp[],
		unsigned sys_nr);


	void waitForEntry(int pid);
	void slurpArgPtrs(int pid, char *const argv[]);
	static void forcePreloads(
		Symbols			*symbols,
		std::set<std::string>	&mmap_fnames,
		const PtrList<ProcMap>& mappings);


	std::map<guest_ptr, uint64_t>	breakpoints;
	mutable Symbols			*symbols; // lazy loaded
	mutable Symbols			*dyn_symbols;
	std::vector<guest_ptr>		argv_ptrs;
	guest_ptr			argc_ptr;
};

#endif
