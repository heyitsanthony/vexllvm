#ifndef GUESTSTATEELF_H
#define GUESTSTATEELF_H

#include "gueststate.h"

class ElfImg;

class GuestStateELF : public GuestState
{
public:
	GuestStateELF(const ElfImg* in_img);
	virtual ~GuestStateELF(void);
	const ElfImg* getExeImage(void) const { return img; }
	llvm::Value* addrVal2Host(llvm::Value* addr_v) const;
	uint64_t addr2Host(guestptr_t guestptr) const;
	guestptr_t name2guest(const char* symname) const;
	void* getEntryPoint(void) const;
	/* copy command line state into state */
	void setArgv(unsigned int argc, const char* argv[]);
private:
	const ElfImg	*img;
	uint8_t		*stack;
};

#endif
