#include <stdio.h>

#include "genllvm.h"
#include "vexxlate.h"
#include "vexsb.h"

using namespace llvm;

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

int main(int argc, char* argv[])
{
	VexXlate	vexxlate;
	VexSB		*vsb;

	theGenLLVM = new GenLLVM();
	printf("LOADING INS\n");
	vsb = vexxlate.xlate(test_str2, TEST_STR2_ADDR);
	printf("LOADING INS DONE\n");
	vsb->print(std::cout);
	vsb->emit();
	delete vsb;
	printf("LLVEX.\n");

	return 0;
}