#include <llvm/Value.h>
#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Intrinsics.h>

#include <queue>


#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "vexxlate.h"
#include "elfimg.h"

extern "C" {
#include <valgrind/libvex.h>
#include <valgrind/libvex_ir.h>
#include <valgrind/libvex_guest_amd64.h>
};

#include "vexsb.h"
#include "genllvm.h"

using namespace llvm;


static std::queue<uint64_t>	jmp_addrs;

static void processAddrs(VexXlate* vexlate, ElfImg* img)
{
	while (!jmp_addrs.empty()) {
		uint64_t	elfaddr;
		elfptr_t	elfptr;
		hostptr_t	hostptr;
		uint64_t	new_jmpaddr;

		elfaddr = jmp_addrs.front();
		jmp_addrs.pop();
		elfptr = (elfptr_t)elfaddr;
		hostptr = img->xlateAddr(elfptr);
		std::cout << "Processing jump addr.." << std::endl;
		std::cout << "ELFENT: " <<  elfaddr << std::endl;
		std::cout << "XLATE: " << hostptr << std::endl;

		VexSB* vsb;
		vsb = vexlate->xlate(hostptr, elfaddr);
		assert (vsb != NULL);
		vsb->print(std::cout);
		vsb->emit();

		new_jmpaddr = vsb->getJmp();

		if (new_jmpaddr) jmp_addrs.push(new_jmpaddr);
		if (vsb->fallsThrough()) jmp_addrs.push(vsb->getEndAddr());

		delete vsb;
	}
}

int main(int argc, char* argv[])
{
	VexXlate	vexlate;
	ElfImg		*img;
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
	theGenLLVM = new GenLLVM();
	jmp_addrs.push((uint64_t)elfentry_pt);
	processAddrs(&vexlate, img);

	printf("LLVEX.\n");

	return 0;
}