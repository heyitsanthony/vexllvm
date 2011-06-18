#ifndef GUESTCPUSTATE_H
#define GUESTCPUSTATE_H

#include <iostream>
#include <stdint.h>
#include "syscallparams.h"
#include <sys/user.h>
#include <assert.h>
#include <map>
#include "elfimg.h"
namespace llvm
{
	class Type;
}

struct guest_ctx_field;

/* types of exit states we need to worry about on return */
enum GuestExitType {
	GE_IGNORE = 0, 	/* use regular control path */
	GE_SIGTRAP = 1,
	GE_SIGSEGV = 2,
	GE_SIGBUS = 3,
	GE_EMWARN = 4
	/* XXX ADD MORE */ };

class GuestTLS;

class GuestCPUState
{
public:
typedef std::map<unsigned int, unsigned int> byte2elem_map;
	static GuestCPUState* create(Arch::Arch arch);
	
	virtual ~GuestCPUState() {};
	virtual const llvm::Type* getTy(void) const = 0;
	virtual unsigned int byteOffset2ElemIdx(unsigned int off) const = 0;
	virtual void* getStateData(void) = 0;
	virtual const void* getStateData(void) const = 0;
	virtual unsigned int getStateSize(void) = 0;
	virtual void setStackPtr(void*) = 0;
	virtual void* getStackPtr(void) const = 0;
	virtual SyscallParams getSyscallParams(void) const = 0;
	virtual void setSyscallResult(uint64_t ret) = 0;
	virtual uint64_t getExitCode(void) const = 0;
	/* byte offset into state data for exit type byte */
	virtual unsigned int getExitTypeOffset(void) const = 0;

	virtual void setExitType(GuestExitType et) = 0;
	virtual GuestExitType getExitType(void) = 0;

	virtual void setFuncArg(uintptr_t arg_val, unsigned int arg_num) = 0;

	virtual void print(std::ostream& os) const = 0;
	virtual const char* off2Name(unsigned int off) const = 0;
private:
};

#endif
