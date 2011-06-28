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

#define SYSCALL_HANDLER(arch, call) SYSCALL_HANDLER_TRICK(arch, call, _)

#define SYSCALL_HANDLER_TRICK(arch, call, underbar) \
	bool arch##underbar##call(		\
		SyscallParams&		args,	\
		GuestMem::Mapping&	m,	\
		unsigned long&		sc_ret)

#define SYSCALL_BODY(arch, call) SYSCALL_BODY_TRICK(arch, call, _)

#define SYSCALL_BODY_TRICK(arch, call, underbar) \
	bool Syscalls::arch##underbar##call(	\
		SyscallParams&		args,	\
		GuestMem::Mapping&	m,	\
		unsigned long&		sc_ret)

class Syscalls
{
public:
	Syscalls(Guest*);
	virtual ~Syscalls();
	virtual uint64_t apply(SyscallParams& args);
	uint64_t apply(void); /* use guest params */
	void print(std::ostream& os) const;
	void print(std::ostream& os, const SyscallParams& sp, 
		uintptr_t *result = NULL) const;
	bool isExit(void) const { return exited; }
	std::string getSyscallName(int guest) const;
	
	int translateSyscall(int guest) const;
protected:
	bool interceptSyscall(
		int sys_nr,
		SyscallParams& args,
		GuestMem::Mapping& m, 
		unsigned long& sc_ret);
	uintptr_t passthroughSyscall(
		SyscallParams& args,
		GuestMem::Mapping& m);

	/* map a guest syscall number to the host equivalent */
	int translateARMSyscall(int sys_nr) const;
	int translateI386Syscall(int sys_nr) const;
	int translateAMD64Syscall(int sys_nr) const;
	
	std::string getARMSyscallName(int sys_nr) const;
	std::string getI386SyscallName(int sys_nr) const;
	std::string getAMD64SyscallName(int sys_nr) const;
	
	/* either passthrough or emulate the syscall */
	uintptr_t applyARMSyscall(
		SyscallParams& args,
		GuestMem::Mapping& m);
	uintptr_t applyI386Syscall(
		SyscallParams& args,
		GuestMem::Mapping& m);
	uintptr_t applyAMD64Syscall(
		SyscallParams& args,
		GuestMem::Mapping& m);
		
	Guest				*guest;
	std::list<SyscallParams>	sc_trace;
	uint64_t			sc_seen_c; /* list.size can be O(n) */
	bool				exited;
	GuestCPUState			*cpu_state;
	GuestMem*			mappings;
	const std::string		binary;
	bool				log_syscalls;
	bool				force_translation;
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
};

#endif
