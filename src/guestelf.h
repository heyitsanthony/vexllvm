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
	llvm::Value* addrVal2Host(llvm::Value* addr_v) const;
	uint64_t addr2Host(guestptr_t guestptr) const;
	guestptr_t name2guest(const char* symname) const;
	void* getEntryPoint(void) const;
	/* copy command line state into state */
	void setArgv(unsigned int argc, const char* argv[],
		int envc, const char* envp[]);

	virtual std::list<GuestMemoryRange*> getMemoryMap(void) const;
	virtual void recordInitialMappings(VexMem& mappings);
private:	
	void setupArgPages();
	void createElfTables(int argc, int envc);
	void copyElfStrings(int argc, const char **argv);
	void loaderBuildArgptr(int envc, int argc,
		guestptr_t stringp, int push_ptr);

	ElfImg	*img;
	uint8_t		*stack;
	std::vector<void*>	arg_pages;
	guestptr_t	arg_stack;
	guestptr_t	stack_base;
	guestptr_t	stack_limit;
	guestptr_t	exe_string;
};

#endif
