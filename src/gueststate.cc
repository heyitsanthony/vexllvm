#include <llvm/Intrinsics.h>
#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Support/IRBuilder.h>

#include <iostream>

#include "elfimg.h"
#include "guestcpustate.h"
#include "gueststate.h"

using namespace llvm;

#define STACK_BYTES	(64*1024)

GuestState::GuestState(const ElfImg* in_img)
: img(in_img)
{
	cpu_state = new GuestCPUState();
	stack = new uint8_t[STACK_BYTES];
	cpu_state->setStackPtr(stack + STACK_BYTES-8);
}

GuestState::~GuestState(void)
{
	delete cpu_state;
	delete [] stack;
}

Value* GuestState::addr2Host(Value* addr_v) const
{
	const ConstantInt	*ci;

	/* cheat if everything could be mapped into the 
	 * right spot. */
	if (img->isDirectMapped()) return addr_v;

	ci = dynamic_cast<const ConstantInt*>(addr_v);
	if (ci != NULL) {
		/* Fast path. Can directly resolve */
		uintptr_t	hostaddr;
		hostaddr = addr2Host(ci->getZExtValue());
		addr_v = ConstantInt::get(
			getGlobalContext(), APInt(64, hostaddr));
		std::cerr << "FAST PATH LOAD" << std::endl;
		return addr_v;
	}

	/* XXX CUT OUT */
	/* Slow path. Need help resolving */
	std::cerr << "SLOW PATH LOAD XXX XXX FIXME" << std::endl;
//	addr_v = new  builder->CreateCall(f, , "__addr2Host");
	addr_v->dump();

	return addr_v;
}

uintptr_t GuestState::addr2Host(uintptr_t guestptr) const
{
	return (uintptr_t)img->xlateAddr((elfptr_t)guestptr);
}
