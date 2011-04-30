#ifndef GUESTSTATE_H
#define GUESTSTATE_H

#include <assert.h>
#include <map>

namespace llvm
{
	class Type;
}

struct guest_ctx_field;

/* TODO: make this a base class when if we want to support other archs */
class GuestState
{
public:
typedef std::map<unsigned int, unsigned int> byte2elem_map;

	GuestState();
	virtual ~GuestState();
	llvm::Type* getTy(void) const { return guestCtxTy; }
	llvm::Value* getLValue(void) const { assert (0 == 1 && "STUB"); }
	unsigned int byteOffset2ElemIdx(unsigned int off) const;
protected:
	llvm::Type* mkFromFields(struct guest_ctx_field* f, byte2elem_map&);
	void mkRegCtx(void);
private:
	byte2elem_map	off2ElemMap;
	llvm::Type	*guestCtxTy;
};

#endif
