#ifndef SC_XLATE_H
#define SC_XLATE_H

#include "syscall/syscalls.h"

#define SYSCALL_BODY(arch, call)	 	\
	bool arch##SyscallXlate::arch##_##call(	\
		Guest&			g,	\
		SyscallParams&		args,	\
		unsigned long&		sc_ret)

class AMD64SyscallXlate : public SyscallXlate
{
public:
	AMD64SyscallXlate() {}

	int translateSyscall(int sys_nr) const override;
	std::string getSyscallName(int sys_nr) const override;
	uintptr_t apply(Guest& g, SyscallParams& args) override;

private:
	SYSCALL_HANDLER(AMD64, arch_prctl);
	SYSCALL_HANDLER(AMD64, arch_modify_ldt);
};

class I386SyscallXlate : public SyscallXlate
{
public:
	I386SyscallXlate() {}

	int translateSyscall(int sys_nr) const override;
	std::string getSyscallName(int sys_nr) const override;
	uintptr_t apply(Guest& g, SyscallParams& args) override;

private:
	SYSCALL_HANDLER(I386, mmap2);
};

class ARMSyscallXlate : public SyscallXlate
{
public:
	ARMSyscallXlate() {}

	int translateSyscall(int sys_nr) const override;
	std::string getSyscallName(int sys_nr) const override;
	uintptr_t apply(Guest& g, SyscallParams& args) override;

private:
	SYSCALL_HANDLER(ARM, cacheflush);
	SYSCALL_HANDLER(ARM, set_tls);
	SYSCALL_HANDLER(ARM, mmap2);
};

#endif
