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
	void* getEntryPoint(void) const;
	/* copy command line state into state */
	void setArgv(unsigned int argc, const char* argv[],
		int envc, const char* envp[]);
	virtual Arch::Arch getArch() const;
private:
	void setupMem(void);
	void setupArgPages();
	void createElfTables(int argc, int envc);
	void copyElfStrings(int argc, const char **argv);
	void loaderBuildArgptr(int envc, int argc,
		guestptr_t stringp, int push_ptr);

	ElfImg			*img;
	std::vector<void*>	arg_pages;
	guestptr_t		arg_stack;
	guestptr_t		stack_base;
	guestptr_t		stack_limit;
	guestptr_t		exe_string;
};

#endif
