#ifndef SYSCALL_NAMER_H
#define SYSCALL_NAMER_H

class Guest;
class SyscallParams;

class SyscallNamer
{
public:
	static const char* xlate(const Guest* g, const SyscallParams& sp);
	static const char* xlate(const Guest* g, unsigned int sysnr);
private:
};

#endif