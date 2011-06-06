#include <llvm/Intrinsics.h>
#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Support/IRBuilder.h>

#include "elfimg.h"
#include "gueststateelf.h"
#include "guestcpustate.h"

using namespace llvm;

#define STACK_BYTES	(64*1024)

GuestStateELF::GuestStateELF(const ElfImg* in_img)
: GuestState(in_img->getFilePath()),
  img(in_img)
{
	stack = new uint8_t[STACK_BYTES];
	memset(stack, 0x32, STACK_BYTES);	/* bogus data */
	cpu_state->setStackPtr(stack + STACK_BYTES-256 /*redzone+gunk*/);
}

GuestStateELF::~GuestStateELF(void) { delete [] stack; }

std::list<GuestMemoryRange*> GuestStateELF::getMemoryMap(void) const
{
	assert (0 == 1 && "STUB");
}

Value* GuestStateELF::addrVal2Host(Value* addr_v) const
{
	const ConstantInt	*ci;

	/* cheat if everything could be mapped into the 
	 * right spot. */
	if (img->isDirectMapped()) return addr_v;

	ci = dynamic_cast<const ConstantInt*>(addr_v);
	if (ci != NULL) {
		/* Fast path. Can directly resolve */
		uintptr_t	hostaddr;
		hostaddr = addr2Host(ci->getZExtValue());
		addr_v = ConstantInt::get(
			getGlobalContext(), APInt(64, hostaddr));
		std::cerr << "FAST PATH LOAD" << std::endl;
		return addr_v;
	}

	/* XXX CUT OUT */
	/* Slow path. Need help resolving */
	std::cerr << "SLOW PATH LOAD XXX XXX FIXME" << std::endl;
	assert (0 == 1 && "SLOW PATH LOAD NOT IMPLEMENTED");
//	addr_v = new  builder->CreateCall(f, , "__addr2Host");
	addr_v->dump();

	return addr_v;
}

uintptr_t GuestStateELF::addr2Host(uintptr_t guestptr) const
{
	return (uintptr_t)img->xlateAddr((elfptr_t)guestptr);
}

guestptr_t GuestStateELF::name2guest(const char* symname) const
{
	return (guestptr_t)img->getSymAddr(symname);
}

void* GuestStateELF::getEntryPoint(void) const { return img->getEntryPoint(); }

/* XXX, amd specified */
#define REDZONE_BYTES	128
/**
 * From libc _start code ...
 *	popq %rsi		; pop the argument count
 * 	movq %rsp, %rdx		; argv starts just at the current stack top.
 * Align the stack to a 16 byte boundary to follow the ABI. 
 *	andq  $~15, %rsp
 *
 */
void GuestStateELF::setArgv(unsigned int argc, const char* argv[])
{
	unsigned int	argv_data_space = 0;
	unsigned int	argv_ptr_space;
	char		*stack_base, *argv_data;
	char		**argv_tab;

	stack_base = (char*)stack + STACK_BYTES;
	stack_base -= REDZONE_BYTES;	/* make room for redzone */
	assert (((uintptr_t)stack_base & 0x7) == 0 && 
		"Stack not 8-aligned. Perf bug!");

	/*  */
	for (unsigned int i = 0; i < argc; i++)
		argv_data_space += strlen(argv[i]) + 1 /* '\0' */;

	/* number of bytes needed to store points to all argv */
	argv_ptr_space = sizeof(const char*) * (argc + 1 /* NULL ptr */);

	/* set s to room for argv strings */
	stack_base -= argv_data_space;
	argv_data = (char*)stack_base;

	/* set argv_tab to beginning of string pointer array argv[] */
	stack_base -= argv_ptr_space;
	argv_tab = (char**)stack_base;
	for (unsigned int i = 0; i < argc; i++) {
		argv_tab[i] = argv_data; /* set argv[i] ptr to stack argv[i] */
		/* copy argv[i] into stack */
		strcpy(argv_data, argv[i]);
		argv_data += strlen(argv[i]);
		argv_data++;		/* skip '\0' */
	}
	argv_tab[argc] = NULL;

	/* store argc */
	stack_base -= sizeof(uintptr_t);
	*((uintptr_t*)stack_base) = argc;

	cpu_state->setStackPtr(stack_base);
}
