/* guest image for a ptrace generated image */
#ifndef GUESTSTATEPTIMG_H
#define GUESTSTATEPTIMG_H

#include <map>
#include "collection.h"
#include "guest.h"
#include "procmap.h"

class Symbols;
class PTImgArch;

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

	template <class T>
	static T* createAttached(
		int pid,
		int argc, char* const argv[], char* const envp[])
	{
		GuestPTImg		*pt_img;
		T			*pt_t;
		pid_t			slurped_pid;

		pt_t = new T(argc, argv, envp, false /* ignore binary */);
		pt_img = pt_t;
		slurped_pid = pt_img->createSlurpedAttach(pid);
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
	virtual Arch::Arch getArch() const { return arch; }

	virtual const Symbols* getSymbols(void) const;
	virtual const Symbols* getDynSymbols(void) const;

	virtual std::vector<guest_ptr> getArgvPtrs(void) const
	{ return argv_ptrs; }

	void setBreakpoint(pid_t pid, guest_ptr addr);
	void resetBreakpoint(pid_t pid, guest_ptr addr);

	PTImgArch* getPTArch(void) { return pt_arch; }

	static Symbols* loadSymbols(const PtrList<ProcMap>& mappings);
	static Symbols* loadDynSymbols(
		GuestMem	*mem,
		const char	*binpath);

protected:
	GuestPTImg(int argc, char* const argv[], char* const envp[],
		bool use_entry=true);
	virtual void handleChild(pid_t pid);

	void slurpRegisters(pid_t pid);

	guest_ptr undoBreakpoint(pid_t pid);

	PTImgArch	*pt_arch;

private:
	pid_t createSlurpedAttach(int pid);
	void attachSyscall(int pid);
	pid_t createSlurpedChild(
		int argc, char *const argv[], char *const envp[]);
	void slurpBrains(pid_t pid);

	guest_ptr			entry_pt;
	PtrList<ProcMap>		mappings;
	std::map<guest_ptr, uint64_t>	breakpoints;
	mutable Symbols			*symbols; // lazy loaded
	mutable Symbols			*dyn_symbols;
	std::vector<guest_ptr>		argv_ptrs;

	Arch::Arch			arch;
};

#endif
