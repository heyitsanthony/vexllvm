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
extern "C" {
#include <valgrind/libvex.h>
#include <valgrind/libvex_ir.h>
#include <valgrind/libvex_guest_amd64.h>
extern void dispatch_asm_amd64(void);
};

#include "vexsb.h"
#include "genllvm.h"

using namespace llvm;

VexGuestAMD64State	reg_state;

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

/*
 * Taken from /bin/ls.
 * dec    %rbx
 * cmp    %rbx,%rax
 * jae    4028d5 <wcstombs@plt+0xb5>
*/
uint8_t test_str[]= {"\x48\xff\xcb\x48\x39\xd8\x73\x24 aaaaaaaaaaaaaaaaaaa"};

/*
4033da:       4c 89 64 24 f0          mov    %r12,-0x10(%rsp)
4033df:       4c 89 6c 24 f8          mov    %r13,-0x8(%rsp)
4033e4:       48 89 f5                mov    %rsi,%rbp
4033e7:       48 83 ec 28             sub    $0x28,%rsp
4033eb:       48 89 fb                mov    %rdi,%rbx
4033ee:       41 89 d5                mov    %edx,%r13d
4033f1:       bf 20 00 00 00          mov    $0x20,%edi
4033f6:       e8 65 d4 00 00          callq  410860 <wcstombs@plt+0xe040>
*/
uint8_t test_str2[] = {
"\x4c\x89\x64\x24\xf0"
"\x4c\x89\x6c\x24\xf8"
"\x48\x89\xf5"
"\x48\x83\xec\x28"
"\x48\x89\xfb"
"\x41\x89\xd5"
"\xbf\x20\x00\x00\x00"
"\xe8\x65\xd4\x00\x00"
"\xe8\x65\xd4\x00\x00"
"\xe8\x65\xd4\x00\x00"
"\xe8\x65\xd4\x00\x00"

};
#define TEST_STR2_ADDR 0x40223a


VexControl		vc;
VexArchInfo		vai_amd64;
VexAbiInfo		vbi;

static void do_xlate(
	const uint8_t* guest_bytes, uint64_t guest_addr)
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

	vta.guest_bytes = const_cast<uint8_t*>(guest_bytes);	/* so stupid */
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
	LibVEX_default_VexControl(&vc);
	LibVEX_Init(vex_exit, vex_log, VEX_DEBUG_LEVEL, false, &vc);

	LibVEX_default_VexArchInfo(&vai_amd64);
	vai_amd64.hwcaps = 0;
	LibVEX_default_VexAbiInfo(&vbi);

	theGenLLVM = new GenLLVM();

	do_xlate(test_str2, TEST_STR2_ADDR);

	printf("LLVEX.\n");

	return 0;
}