#ifndef GUESTCPUSTATE_H
#define GUESTCPUSTATE_H

#include <iostream>
#include <stdint.h>
#include "syscallparams.h"

#include <assert.h>
#include <map>

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

/* TODO: make this a base class when if we want to support other archs */
class GuestCPUState
{
public:
typedef std::map<unsigned int, unsigned int> byte2elem_map;

	GuestCPUState();
	virtual ~GuestCPUState();
	const llvm::Type* getTy(void) const { return guestCtxTy; }
	unsigned int byteOffset2ElemIdx(unsigned int off) const;
	void* getStateData(void) { return state_data; }
	const void* getStateData(void) const { return state_data; }
	unsigned int getStateSize(void) { return state_byte_c; }
	void setStackPtr(void*);
	void* getStackPtr(void) const;
	SyscallParams getSyscallParams(void) const;
	void setSyscallResult(uint64_t ret);
	uint64_t getExitCode(void) const;
	/* byte offset into state data for exit type byte */
	unsigned int getExitTypeOffset(void) const { return state_byte_c; }

	void setExitType(GuestExitType et) { *exit_type = (uint8_t)et; }
	GuestExitType getExitType(void) { return (GuestExitType)*exit_type; }

	void setFuncArg(uintptr_t arg_val, unsigned int arg_num);

	void print(std::ostream& os) const;

	GuestTLS* getTLS(void) { return tls; }
	const GuestTLS* getTLS(void) const { return tls; }
protected:
	llvm::Type* mkFromFields(struct guest_ctx_field* f, byte2elem_map&);
	void mkRegCtx(void);
private:
	byte2elem_map	off2ElemMap;
	llvm::Type	*guestCtxTy;
	uint8_t		*state_data;	/* amd64 guest + exit_type */
	uint8_t		*exit_type;	/* ptr into state data */

	GuestTLS	*tls;
	unsigned int	state_byte_c;
};

#endif
