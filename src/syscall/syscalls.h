#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <iostream>
#include <list>
#include <vector>

#include "syscallparams.h"
#include "guestmem.h"

#define MAX_SC_TRACE	1000

class Guest;
class GuestCPUState;

#define SYSCALL_HANDLER(arch, call)		\
	bool arch##_##call(			\
		SyscallParams&		args,	\
		unsigned long&		sc_ret)

#define SYSCALL_BODY(arch, call)	 \
	bool Syscalls::arch##_##call(	\
		SyscallParams&		args,	\
		unsigned long&		sc_ret)

class Syscalls
{
public:
	virtual ~Syscalls();
	virtual uint64_t apply(SyscallParams& args);
	uint64_t apply(void); /* use guest params */
	void print(std::ostream& os) const;
	void print(std::ostream& os, const SyscallParams& sp, 
		uintptr_t *result = NULL) const;
	bool isExit(void) const { return exited; }
	std::string getSyscallName(int guest) const;
	
	int translateSyscall(int guest) const;

	static Syscalls* create(Guest*);
protected:
	Syscalls(Guest*);

	bool interceptSyscall(
		int sys_nr,
		SyscallParams& args,
		unsigned long& sc_ret);

	bool tryPassthrough(SyscallParams& args, uintptr_t& sc_ret);
	uintptr_t passthroughSyscall(SyscallParams& args);

	/* map a guest syscall number to the host equivalent */
	int translateARMSyscall(int sys_nr) const;
	int translateI386Syscall(int sys_nr) const;
	int translateAMD64Syscall(int sys_nr) const;
	
	std::string getARMSyscallName(int sys_nr) const;
	std::string getI386SyscallName(int sys_nr) const;
	std::string getAMD64SyscallName(int sys_nr) const;
	
	/* either passthrough or emulate the syscall */
	uintptr_t applyARMSyscall(SyscallParams& args);
	uintptr_t applyI386Syscall(SyscallParams& args);
	uintptr_t applyAMD64Syscall(SyscallParams& args);
		
	Guest				*guest;
	std::list<SyscallParams>	sc_trace;
	uint64_t			sc_seen_c; /* list.size can be O(n) */
	bool				exited;
	GuestCPUState			*cpu_state;
	GuestMem*			mappings;
	const std::string		binary;
	bool				log_syscalls;
	bool				force_qemu_syscalls;
public:
	const static std::string	chroot;
	
/* specific architecture handling of syscall could go in another
   class if there wasn't the marshalled syscall option.  so we'll
   dispatch to other functins for platform specific stuff.  these can
   go into separate files if they get big */
private:
	SYSCALL_HANDLER(AMD64, arch_prctl);
	SYSCALL_HANDLER(AMD64, arch_modify_ldt);
	SYSCALL_HANDLER(ARM, cacheflush);
	SYSCALL_HANDLER(ARM, set_tls);
	SYSCALL_HANDLER(ARM, mmap2);
	SYSCALL_HANDLER(I386, mmap2);
};

#endif
