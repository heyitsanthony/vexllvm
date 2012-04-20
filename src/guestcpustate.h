#ifndef GUESTCPUSTATE_H
#define GUESTCPUSTATE_H

#include <iostream>
#include <stdint.h>
#include "syscall/syscallparams.h"
#include "arch.h"
#include <sys/user.h>
#include <assert.h>
#include <map>

#include "guestptr.h"

namespace llvm
{
	class Type;
}

struct guest_ctx_field
{
	unsigned char	f_len;
	unsigned int	f_count;
	const char*	f_name;
};

/* types of exit states we need to worry about on return */
enum GuestExitType {
	GE_IGNORE = 0, 	/* use regular control path */
	GE_SIGTRAP = 1,
	GE_SIGSEGV = 2,
	GE_SIGBUS = 3,
	GE_EMWARN = 4,
	GE_SYSCALL = 5,
	GE_CALL = 6,
	GE_RETURN = 7
	/* XXX ADD MORE */ };

class GuestCPUState
{
public:
typedef std::map<unsigned int, unsigned int> byte2elem_map;
	static GuestCPUState* create(Arch::Arch arch);
	GuestCPUState();
	virtual ~GuestCPUState() {};

	llvm::Type* getTy(void) const { return guestCtxTy; }
	virtual unsigned int byteOffset2ElemIdx(unsigned int off) const = 0;

	void* getStateData(void) { return state_data; }
	const void* getStateData(void) const { return state_data; }
	unsigned int getStateSize(void) const { return state_byte_c+1; }
	uint8_t* copyStateData(void) const;

	virtual void setStackPtr(guest_ptr) = 0;
	virtual guest_ptr getStackPtr(void) const = 0;
	virtual void setPC(guest_ptr) = 0;
	virtual guest_ptr getPC(void) const = 0;
	virtual SyscallParams getSyscallParams(void) const = 0;
	virtual void setSyscallResult(uint64_t ret) = 0;
	virtual uint64_t getExitCode(void) const = 0;

	/* byte offset into state data for exit type byte */
	unsigned int getExitTypeOffset(void) const { return state_byte_c; }
	void setExitType(GuestExitType et) { *exit_type = (uint8_t)et; }
	GuestExitType getExitType(void) { return (GuestExitType)*exit_type; }

	virtual void setFuncArg(uintptr_t arg_val, unsigned int arg_num) = 0;
	virtual unsigned int getFuncArgOff(unsigned int arg_num) const
	{ assert (0 == 1 && "STUB"); return 0; }
	virtual unsigned int getRetOff(void) const
	{ assert (0 == 1 && "STUB"); return 0; }
	virtual unsigned int getStackRegOff(void) const
	{ assert (0 == 1 && "STUB"); return 0; }
	virtual unsigned int getPCOff(void) const
	{ assert (0 == 1 && "STUB"); return 0; }


	virtual void resetSyscall(void)
	{ assert (0 == 1 && "STUB"); }

	void print(std::ostream& os) const;
	virtual void print(std::ostream& os, const void* regctx) const = 0;

	virtual const char* off2Name(unsigned int off) const = 0;

	virtual bool load(const char* fname);
	virtual bool save(const char* fname);

	/* returns byte offset into raw cpu data that maps to
	 * corresponding gdb register file offset; returns -1
	 * if exceeds bounds */
	virtual int cpu2gdb(int gdb_off) const
	{ assert (0 ==1 && "STUB"); return -1; }
protected:
	llvm::Type* mkFromFields(struct guest_ctx_field* f, byte2elem_map&);
protected:
	byte2elem_map	off2ElemMap;
	llvm::Type*	guestCtxTy;
	uint8_t		*state_data;
	uint8_t		*exit_type;
	unsigned int	state_byte_c;
};

#endif
