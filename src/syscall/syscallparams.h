#ifndef SYSCALLPARAMS_H
#define SYSCALLPARAMS_H

#include <stdint.h>
#include <assert.h>

#define NUM_SYSCALL_ARGS	6

/* Abstract layer for passing around syscall params */
class SyscallParams
{
public:
	 /* XXX how many? args */
	SyscallParams(
		uintptr_t sc_num, 
		uintptr_t arg1, uintptr_t arg2, uintptr_t arg3,
		uintptr_t arg4, uintptr_t arg5, uintptr_t arg6)
	: sc(sc_num)
	{
		args[0] = arg1;
		args[1] = arg2;
		args[2] = arg3;
		args[3] = arg4;
		args[4] = arg5;
		args[5] = arg6;
	}

	virtual ~SyscallParams() {}
	unsigned int getSyscall(void) const { return sc; }
	uintptr_t getArg(unsigned int n) const
	{
		assert (n < NUM_SYSCALL_ARGS);
		return args[n];
	}

	void* getArgPtr(unsigned int n) const { return (void*)getArg(n); }

	void setArg(unsigned int n, uintptr_t v)
	{
		assert (n < NUM_SYSCALL_ARGS);
		args[n] = v;
	}
private:
	uintptr_t	sc;
	uintptr_t	args[NUM_SYSCALL_ARGS];
};

#endif