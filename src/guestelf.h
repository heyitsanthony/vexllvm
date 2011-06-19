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
		char* stringp, int push_ptr);
		
	void pushPadByte();
	void pushPointer(const void*);
	void pushNative(uintptr_t);
	void putPointer(char*& p, const void* v, ssize_t inc = 0);
	void putNative(char*& p, uintptr_t v, ssize_t inc = 0);
	void nextNative(char*& p, ssize_t inc = 1);

	ElfImg			*img;
	std::vector<char*>	arg_pages;
	char*			sp;
	size_t			arg_stack;
	char*			stack_base;
	char*			stack_limit;
	char*			exe_string;
	std::string		ld_library_path;
};

#endif
