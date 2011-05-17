#include <iostream>
#include <string.h>

#include "util.h"
#include "guestcpustate.h"
#include "gueststate.h"

using namespace llvm;

#define STACK_BYTES	(64*1024)

GuestState::GuestState(void)
{
	cpu_state = new GuestCPUState();
	stack = new uint8_t[STACK_BYTES];
	memset(stack, 0x32, STACK_BYTES);	/* bogus data */
	cpu_state->setStackPtr(stack + STACK_BYTES-256 /*redzone+gunk*/);
}

GuestState::~GuestState(void)
{
	delete cpu_state;
	delete [] stack;
}

SyscallParams GuestState::getSyscallParams(void) const
{
	return cpu_state->getSyscallParams();
}

void GuestState::setSyscallResult(uint64_t ret)
{
	cpu_state->setSyscallResult(ret);
}

std::string GuestState::getName(guestptr_t x) const { return hex_to_string(x); }

uint64_t GuestState::getExitCode(void) const
{
	return cpu_state->getExitCode();
}

void GuestState::print(std::ostream& os) const { cpu_state->print(os); }


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
void GuestState::setArgv(unsigned int argc, const char* argv[])
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
