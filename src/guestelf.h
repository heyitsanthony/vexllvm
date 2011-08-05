#ifndef GUESTSTATEELF_H
#define GUESTSTATEELF_H

#include "guest.h"
#include <vector>

class ElfImg;

class GuestELF : public Guest
{
public:
	GuestELF(ElfImg* in_img);
	virtual ~GuestELF(void);
	const ElfImg* getExeImage(void) const { return img; }
	guest_ptr getEntryPoint(void) const;
	/* copy command line state into state */
	void setArgv(unsigned int argc, const char* argv[],
		int envc, const char* envp[]);
	virtual Arch::Arch getArch() const;

	virtual std::vector<guest_ptr> getArgvPtrs(void) const
	{ return argv_ptrs; }

private:
	void setupMem(void);
	void setupArgPages();
	void createElfTables(int argc, int envc);
	void copyElfStrings(int argc, const char **argv);
	void loaderBuildArgptr(int envc, int argc,
		guest_ptr stringp, int push_ptr);
		
	void pushPadByte();
	void pushPointer(guest_ptr);
	void pushNative(uintptr_t);
	void putPointer(guest_ptr& p, guest_ptr v, ssize_t inc = 0);
	void putNative(guest_ptr& p, uintptr_t v, ssize_t inc = 0);
	void nextNative(guest_ptr& p, ssize_t inc = 1);

	ElfImg			*img;
	std::vector<char*>	arg_pages;
	guest_ptr		sp;
	size_t			arg_stack;
	guest_ptr		stack_base;
	guest_ptr		stack_limit;
	guest_ptr		exe_string;
	std::string		ld_library_path;

	std::vector<guest_ptr>	argv_ptrs;
};

#endif
