#ifndef SYSCALLPARAMS_H
#define SYSCALLPARAMS_H

#include <stdint.h>
#include <assert.h>

#define NUM_SYSCALL_ARGS	3

/* Abstract layer for passing around syscall params */
class SyscallParams
{
public:
	 /* XXX how many? args */
	SyscallParams(
		uintptr_t sc_num, 
		uintptr_t arg1, uintptr_t arg2, uintptr_t arg3)
	: sc(sc_num)
	{
		args[0] = arg1;
		args[1] = arg2;
		args[2] = arg3;
	}

	virtual ~SyscallParams() {}
	unsigned int getSyscall(void) const { return sc; }
	uintptr_t getArg(unsigned int n) const
	{
		assert (n < NUM_SYSCALL_ARGS);
		return args[n];
	}
private:
	uintptr_t	sc;
	uintptr_t	args[NUM_SYSCALL_ARGS];
};

#endif