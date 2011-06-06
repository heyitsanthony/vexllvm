#ifndef SYSCALLSMARSHALLED_H
#define SYSCALLSMARSHALLED_H

#include "syscalls.h"

class SyscallPtrBuf
{
public:
	SyscallPtrBuf(unsigned int in_len, void* in_ptr);
	virtual ~SyscallPtrBuf(void) { if (data) delete [] data; }
	void* getPtr(void) const { return ptr; }
	const void* getData(void) const { return data; }
	unsigned int getLength(void) const { return len; }
private:
	void		*ptr;
	unsigned int	len;
	char		*data;
};

class SyscallsMarshalled : public Syscalls
{
public:
	SyscallsMarshalled(VexMem& mappings);
	virtual ~SyscallsMarshalled()
	{
		if (last_sc_ptrbuf) delete last_sc_ptrbuf;
	}
	virtual uint64_t apply(SyscallParams& args);
	SyscallPtrBuf *takePtrBuf(void)
	{
		SyscallPtrBuf	*ret = last_sc_ptrbuf;
		last_sc_ptrbuf = NULL;
		return ret;
	}

	bool isSyscallMarshalled(int sys_nr) const;
private:
	SyscallPtrBuf	*last_sc_ptrbuf;
	bool		log_syscalls;
};

#endif

