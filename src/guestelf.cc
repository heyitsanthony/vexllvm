#include <llvm/Intrinsics.h>
#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Support/IRBuilder.h>
#include <sys/mman.h>
#include <fstream>
#include <fcntl.h>
#include <sstream>

#include "elfimg.h"
#include "elfsegment.h"
#include "guestelf.h"
#include "guestcpustate.h"
#include "Sugar.h"

using namespace llvm;

#define STACK_BYTES (2 * 1024 * 1024)
/* these macros are largely cheap shortcuts to get something working.
   eventually, the code in here will do a better job discerning between
   guest and host addresses, etc */
#define MAX_ARG_PAGES	32


GuestELF::GuestELF(ElfImg* in_img)
: Guest(in_img->getFilePath())
, img(in_img)
, arg_pages(MAX_ARG_PAGES)
{
	cpu_state = GuestCPUState::create(img->getArch());
}

GuestELF::~GuestELF(void) {}

void* GuestELF::getEntryPoint(void) const { 
	if(img->getInterp())
		return img->getInterp()->getEntryPoint(); 
	else
		return img->getEntryPoint(); 
}

/* XXX, amd specified */
#define REDZONE_BYTES	128
/**
 * From libc _start code ...
 *	popq %rsi		; pop the argument count
 *	movq %rsp, %rdx		; argv starts just at the current stack top.
 * Align the stack to a 16 byte boundary to follow the ABI. 
 *	andq  $~15, %rsp
 *
 */

#define page_num(x) (uintptr_t)(x) / img->getPageSize()
#define page_off(x) (uintptr_t)(x) % img->getPageSize()
//borrowed liberally from qemu
void GuestELF::copyElfStrings(int argc, const char **argv)
{
	const char *tmp, *tmp1;
	char *pag = NULL;
	int len, offset = 0;

	assert(arg_stack);

	while (argc-- > 0) {
		tmp = argv[argc];

		assert(tmp && "VFS: argc is wrong");

		tmp1 = tmp;
		while (*tmp++);
		len = tmp - tmp1;
		
		assert(arg_stack >= (unsigned int)len);
		while (len) {
			--arg_stack; --tmp; --len;
			if (--offset < 0) {
				offset = page_off(arg_stack);
				pag = arg_pages[page_num(arg_stack)];
				if (!pag) {
					pag = new char[img->getPageSize()];
					memset(pag, 0, img->getPageSize());
					arg_pages[page_num(arg_stack)] = pag;
					if (!pag)
						return;
				}
			}
			if (len == 0 || offset == 0) {
				*(pag + offset) = *tmp;
			}
			else {
				int bytes_to_copy = (len > offset) ? 
					offset : len;
				tmp -= bytes_to_copy;
				arg_stack -= bytes_to_copy;
				offset -= bytes_to_copy;
				len -= bytes_to_copy;
				memcpy(pag + offset, tmp, bytes_to_copy + 1);
			}
		}
	}
}

//borrowed liberally from qemu
void GuestELF::setupArgPages()
{
	std::size_t size, guard; 
	char *error;

	/* Create enough stack to hold everything.	If we don't use
	it for args, we'll use it for something else.  */
	size = STACK_BYTES;
	if (size < MAX_ARG_PAGES*img->getPageSize()) {
		size = MAX_ARG_PAGES*img->getPageSize();
	}
	/* TODO: this check and math really isn't right */
	guard = img->getPageSize();
	if (guard < PAGE_SIZE) {
		guard = PAGE_SIZE;
	}

	int flags = MAP_PRIVATE | MAP_ANONYMOUS;
	if(img->getAddressBits() == 32)
		flags |= MAP_32BIT;
		
	error = (char*)mmap(0, size + guard, PROT_READ | PROT_WRITE,
		flags, -1, 0);
	assert(error != MAP_FAILED);

	/* We reserve one extra page at the top of the stack as guard.	*/
	mprotect(error, guard, PROT_NONE);

	stack_limit = error + guard;
	stack_base = stack_limit + size - MAX_ARG_PAGES*img->getPageSize();
	sp = stack_base + arg_stack;

	foreach(it, arg_pages.begin(), arg_pages.end()) {
		if (*it) {
			// info->rss++;
			memcpy(stack_base, *it, img->getPageSize());
			delete [] *it;
			*it = NULL;
		}
		stack_base += img->getPageSize();
	}

	if(getenv("VEXLLVM_LOG_MAPPINGS")) {
		std::cerr << "stack @ " << (void*)stack_limit << " sz " 
			<< (void*)size << std::endl;
	}
}

/* dunno where to get these that i know they will be on 
   any system, so just stickem here */
#define ARM_HWCAP_SWP       1
#define ARM_HWCAP_HALF      2
#define ARM_HWCAP_THUMB     4
#define ARM_HWCAP_26BIT     8       /* Play it safe */
#define ARM_HWCAP_FAST_MULT 16
#define ARM_HWCAP_FPA       32
#define ARM_HWCAP_VFP       64
#define ARM_HWCAP_EDSP      128
#define ARM_HWCAP_JAVA      256
#define ARM_HWCAP_IWMMXT    512
#define ARM_HWCAP_CRUNCH    1024
#define ARM_HWCAP_THUMBEE   2048
#define ARM_HWCAP_NEON      4096
#define ARM_HWCAP_VFPv3     8192
#define ARM_HWCAP_VFPv3D16  16384
#define ARM_HWCAP_TLS       32768


void GuestELF::createElfTables(int argc, int envc)
{
	char* string_stack = sp;
	int size;
	char* u_platform;
	const char *k_platform = NULL;

#ifdef CONFIG_USE_FDPIC
	/* Needs to be before we load the env/argc/... */
	if (elf_is_fdpic(exec)) {
		/* Need 4 byte alignment for these structs */
		sp &= ~3;
		sp = loader_build_fdpic_loadmap(info, sp);
		info->other_info = interp_info;
		if (interp_info) {
			interp_info->other_info = info;
			sp = loader_build_fdpic_loadmap(
				interp_info, sp);
		}
	}
#endif
	/* add a platform string */
	u_platform = 0;
	switch(img->getArch()) {
	case Arch::X86_64:
		k_platform = "x86_64";
		break;
	case Arch::I386:
		k_platform = "i386";
		break;
	case Arch::ARM:
		k_platform = "ARM";
		break;
	}
	if (k_platform) {
		size_t len = strlen(k_platform) + 1;
		sp -= 8;
		memcpy(sp, k_platform, len);
		u_platform = sp;
	}
	
	/* random bytes used for aslr */
	void* random_bytes = sp - 16;
	int rnd = open("/dev/urandom", O_RDONLY);
	rnd_br = read(rnd, (void*)random_bytes, 16);
	assert (rnd_br == 16 && "Bad urandom read");
	close(rnd);
	sp -= 16;

	/* align to 16 bytes for shits and giggles */
	while((uintptr_t)sp & 0xf) 
		pushPadByte();

	pushNative(0);				pushNative(AT_NULL);
	pushPointer(u_platform);		pushNative(AT_PLATFORM);
	pushPointer(exe_string);		pushNative(AT_EXECFN);	
	pushPointer(random_bytes);		pushNative(AT_RANDOM);	
	pushNative(0); 				pushNative(AT_SECURE);
	pushNative(getegid());			pushNative(AT_EGID);
	pushNative(getgid());			pushNative(AT_GID);
	pushNative(geteuid());			pushNative(AT_EUID);
	pushNative(getuid());			pushNative(AT_UID);
	pushPointer(img->getEntryPoint());	pushNative(AT_ENTRY); 
	pushNative(0);			 	pushNative(AT_FLAGS); 

	pushPointer(img->getInterp() ?
		img->getInterp()->getFirstSegment()->relocation()
		: img->getFirstSegment()->relocation());
	pushNative(AT_BASE);	

	pushNative(img->getHeaderCount());	pushNative(AT_PHNUM);
	pushNative(img->getElfPhdrSize());	pushNative(AT_PHENT);
	pushPointer(img->getHeader());		pushNative(AT_PHDR);
	pushNative(sysconf(_SC_CLK_TCK)); 	pushNative(AT_CLKTCK);
	pushNative(img->getPageSize());		pushNative(AT_PAGESZ);

	switch(img->getArch()) {
	case Arch::ARM:
		/* more? */
		pushNative(ARM_HWCAP_THUMB | ARM_HWCAP_NEON | 
			ARM_HWCAP_VFPv3 | ARM_HWCAP_TLS);
		break;
	case Arch::I386:
		/* check me!!! stolen from below*/
		pushNative(0x000000000febfbff);
		break;
	case Arch::X86_64:
		/* less? */
		pushNative(0x000000000febfbff);
		break;
	case Arch::Unknown:
		/* less? */
		pushNative(0);
		break;
	}
	pushNative(AT_HWCAP);

	//ptr to syscall - need to fetch, or stick a dummy one in?
	//pushNative(AT_SYSINFO_EHDR); pushPointer(?????); 
	//fd for exe
	// pushNative(AT_EXECFD); pushNative(???);

	loaderBuildArgptr(envc, argc, string_stack, 0);
}

/* Construct the envp and argv tables on the target stack.	*/
void GuestELF::loaderBuildArgptr(int envc, int argc,
	char* stringp, int push_ptr)
{
	char* envp;
	char* argv;

	/* reserve space for the environment string pointers and a null */
	for(int i = 0; i <= envc; ++i) pushPointer(NULL);
	envp = sp;

	/* reserve space for the argument string pointers and a null */
	for(int i = 0; i <= argc; ++i) pushPointer(NULL);
	argv = sp;

	/* our ABI doesn't need this */
	if (push_ptr) {
		pushPointer(envp);
		pushPointer(argv);
	}
	
	pushNative(argc);
	
	/* note where we store the bin path, because the elf tables want it */
	exe_string = stringp;
	
	/* copy all the arg pointers into the table */
	while (argc-- > 0) {
		putPointer(argv, stringp, 1);
		stringp += strlen((char*)stringp) + 1;
	}

	/* copy all the env pointers into the table */
	while (envc-- > 0) {
		putPointer(envp, stringp, 1);
		stringp += strlen((char*)stringp) + 1;
	}
}

void GuestELF::setArgv(unsigned int argc, const char* argv[],
	int envc, const char* envp[])
{
	const char* filename;

	arg_stack = img->getPageSize()*MAX_ARG_PAGES-sizeof(void*);
	filename = getBinaryPath();

	copyElfStrings(1, &filename);
	// blank environ for now
	copyElfStrings(envc, envp);
	copyElfStrings(argc, argv);
	
	setupArgPages();

	createElfTables(argc, envc);
	
	if(getenv("VEXLLVM_DUMP_MAPS")) {
		std::list<ElfSegment*> m;
		img->getSegments(m);
		foreach(it, m.begin(), m.end()) {
			std::ostringstream save_fname;
			save_fname << argv[0] << "." << getpid() 
				<< "." << (*it)->base();
			std::ofstream o(save_fname.str().c_str());
			o.write((char*)(*it)->base(), (*it)->length());
		}
		std::ostringstream save_fname;
		save_fname << argv[0] << "." << getpid() 
			<< "." << (void*)stack_limit;
		std::ofstream o(save_fname.str().c_str());
		o.write(stack_limit, stack_base - stack_limit);
	}

	cpu_state->setStackPtr((void*)sp);
	setupMem();
}

void GuestELF::setupMem(void)
{
	std::list<ElfSegment*>	m;
	void			*top_brick;

	assert (mem == NULL);
	mem = new GuestMem();

	img->getSegments(m);

	top_brick = NULL;
	foreach(it, m.begin(), m.end()) {
		GuestMem::Mapping s(
			(*it)->base(),
			(*it)->length(),
			(*it)->protection());
		mem->recordMapping(s);
		top_brick = s.end();
	}

	/* is this actually computed properly? */
	mem->sbrk(top_brick);

	/* also record the stack */
	GuestMem::Mapping s(
		(void*)stack_limit,
		stack_base - stack_limit,
		PROT_READ | PROT_WRITE,
		true);
	mem->recordMapping(s);
}
Arch::Arch GuestELF::getArch() const {
	return img->getArch();
}

void GuestELF::pushPointer(const void* v) {
	pushNative((uintptr_t)v);
}
void GuestELF::pushNative(uintptr_t v) {
	if(img->getAddressBits() == 32) {
		assert(!(v & ~0xFFFFFFFFULL));
		sp -= sizeof(unsigned int);
		*(unsigned*)sp = v;
	} else {
		/* only other option is 64-bit and that means
		   we are on a 64-bit host, so smash it in there*/
		sp -= sizeof(uintptr_t);
		*(uintptr_t*)sp = v;
	}
}
void GuestELF::putPointer(char*& p, const void* v, ssize_t inc) {
	putNative(p, (uintptr_t)v, inc);
}
void GuestELF::putNative(char*& p, uintptr_t v, ssize_t inc) {
	if(img->getAddressBits() == 32) {
		assert(!(v & ~0xFFFFFFFFULL));
		*(unsigned*)p = v;
	} else {
		/* only other option is 64-bit and that means
		   we are on a 64-bit host, so smash it in there*/
		*(uintptr_t*)p = v;
	}
	nextNative(p, inc);
}	
void GuestELF::nextNative(char*& p, ssize_t num) {
	if(img->getAddressBits() == 32) {
		p += num * sizeof(unsigned int);
	} else {
		/* only other option is 64-bit and that means
		   we are on a 64-bit host, so smash it in there*/
		p += num * sizeof(uintptr_t);
	}
}

void GuestELF::pushPadByte() {
	*--sp = 0;
}
