#include <llvm/Intrinsics.h>
#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Support/IRBuilder.h>

#include "elfimg.h"
#include "gueststateelf.h"

using namespace llvm;

Value* GuestStateELF::addrVal2Host(Value* addr_v) const
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

uintptr_t GuestStateELF::addr2Host(uintptr_t guestptr) const
{
	return (uintptr_t)img->xlateAddr((elfptr_t)guestptr);
}

guestptr_t GuestStateELF::name2guest(const char* symname) const
{
	return (guestptr_t)img->getSymAddr(symname);
}

void* GuestStateELF::getEntryPoint(void) const { return img->getEntryPoint(); }
