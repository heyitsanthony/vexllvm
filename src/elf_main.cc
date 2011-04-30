#include <llvm/Value.h>
#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Intrinsics.h>

#define VEX_DEBUG_LEVEL	0
#define VEX_TRACE_FLAGS	0


#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "elfimg.h"

extern "C" {
#include <valgrind/libvex.h>
#include <valgrind/libvex_ir.h>
#include <valgrind/libvex_guest_amd64.h>
extern void dispatch_asm_amd64(void);
};

#include "vexsb.h"
#include "genllvm.h"

using namespace llvm;

__attribute__((noreturn)) void vex_exit(void)
{
	printf("FAILURE EXIT\n");
	exit(1);
}

void vex_log(HChar* hc, Int nbytes) { printf("%s", hc); }

Bool vex_chase_ok(void* cb, Addr64 x) { return false; }

uint64_t last_addr;

IRSB* vex_final_irsb(IRSB* irsb)
{
	ppIRSB(irsb);
	printf("LOADING INS\n");
	VexSB	*vsb = new VexSB(last_addr, irsb);
	printf("LOADING INS DONE\n");
	vsb->print(std::cout);
	vsb->emit();
	return irsb;
}

VexControl		vc;
VexArchInfo		vai_amd64;
VexAbiInfo		vbi;

static void do_xlate(
	const void* guest_bytes, uint64_t guest_addr)
{
	VexTranslateArgs	vta;
	VexGuestExtents		vge;
	VexTranslateResult	res;
	Int			host_bytes_used;	/* WHY A PTR?? */
	uint8_t			b[5000];

	last_addr = guest_addr;
	memset(&vta, 0, sizeof(vta));
	vta.arch_guest = VexArchAMD64;
	vta.archinfo_guest = vai_amd64;
	vta.arch_host = VexArchAMD64;
	vta.archinfo_host = vai_amd64;
	vbi.guest_stack_redzone_size = 128;	/* I LOVE RED ZONE. BEST ABI BEST.*/
	vta.abiinfo_both = vbi;

	vta.callback_opaque = NULL;		/* no callback yet */

	vta.guest_bytes = static_cast<uint8_t*>(
		const_cast<void*>(guest_bytes));	/* so stupid */
	vta.guest_bytes_addr = guest_addr; /* where guest thinks it is */

	vta.chase_into_ok = vex_chase_ok;	/* not OK */
	vta.guest_extents = &vge;

	/* UGH. DO NOT DECODE. YELL AT LIBVEX */
	vta.host_bytes = b;
	vta.host_bytes_size = 0;
	vta.host_bytes_used = &host_bytes_used;

	vta.finaltidy = vex_final_irsb;
	vta.traceflags = VEX_TRACE_FLAGS;
	vta.dispatch = (void*)dispatch_asm_amd64;
	printf("DISP: %p\n", dispatch_asm_amd64);

	res = LibVEX_Translate(&vta);
}

int main(int argc, char* argv[])
{
	ElfImg		*img;
	hostptr_t	hostentry_pt;
	elfptr_t	elfentry_pt;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s elf_path\n", argv[0]);
		return -1;
	}

	img = ElfImg::create(argv[1]);
	if (img == NULL) {
		fprintf(stderr, "%s: Could not open ELF %s\n", 
			argv[0], argv[1]);
		return -2;
	}


	elfentry_pt = img->getEntryPoint();
	hostentry_pt = img->xlateAddr(elfentry_pt);

	std::cout << "ELFENT: " <<  elfentry_pt << std::endl;
	std::cout << "XLATE: " << hostentry_pt << std::endl;

	LibVEX_default_VexControl(&vc);
	LibVEX_Init(vex_exit, vex_log, VEX_DEBUG_LEVEL, false, &vc);

	LibVEX_default_VexArchInfo(&vai_amd64);
	vai_amd64.hwcaps = 0;
	LibVEX_default_VexAbiInfo(&vbi);

	theGenLLVM = new GenLLVM();

	do_xlate(hostentry_pt, (uint64_t)elfentry_pt);

	printf("LLVEX.\n");

	return 0;
}