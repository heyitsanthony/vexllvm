#ifndef GUESTCPUSTATE_H
#define GUESTCPUSTATE_H

#include "syscallparams.h"

#include <assert.h>
#include <map>

namespace llvm
{
	class Type;
}

struct guest_ctx_field;

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
	void setStackPtr(void*);
	SyscallParams getSyscallParams(void) const;
	void setSyscallResult(uint64_t ret);
protected:
	llvm::Type* mkFromFields(struct guest_ctx_field* f, byte2elem_map&);
	void mkRegCtx(void);
private:
	byte2elem_map	off2ElemMap;
	llvm::Type	*guestCtxTy;
	uint8_t		*state_data;
	uint8_t		*tls_data;	/* so %fs works */
	unsigned int	state_byte_c;
};

#endif
