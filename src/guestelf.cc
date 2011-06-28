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


GuestELF::GuestELF(GuestMem* mem, ElfImg* in_img)
: Guest(mem, in_img->getFilePath())
, img(in_img)
, arg_pages(MAX_ARG_PAGES)
{
	cpu_state = GuestCPUState::create(mem, img->getArch());
}

GuestELF::~GuestELF(void) {}

guest_ptr GuestELF::getEntryPoint(void) const { 
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
	guest_ptr error;

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
		
	int res = mem->mmap(error, guest_ptr(0), size + guard, 
		PROT_READ | PROT_WRITE, flags, -1, 0);
	assert(!res && "failed to map stack");

	/* We reserve one extra page at the top of the stack as guard.	*/
	mem->mprotect(error, guard, PROT_NONE);

	stack_limit = error + guard;
	stack_base = guest_ptr(
		stack_limit.o + size - MAX_ARG_PAGES*img->getPageSize());
	sp = stack_base + arg_stack;

	foreach(it, arg_pages.begin(), arg_pages.end()) {
		if (*it) {
			// info->rss++;
			mem->memcpy(stack_base, *it, img->getPageSize());
			delete [] *it;
			*it = NULL;
		}
		stack_base.o += img->getPageSize();
	}
	exe_string = sp;

	if(getenv("VEXLLVM_LOG_MAPPINGS")) {
		std::cerr << "stack @ " << (void*)stack_limit.o << " sz " 
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

class ElfTable : public std::vector<std::pair<uintptr_t, uintptr_t> >
{
public:
	template <typename T>
	void add(uintptr_t t, T v) {
		assert(!((uintptr_t)v & ~0xFFFFFFFFULL));
		push_back(std::make_pair(t, (uintptr_t)v));
	}
};

void GuestELF::createElfTables(int argc, int envc)
{
	guest_ptr string_stack = sp;
	int rnd_br;
	guest_ptr u_platform(0);
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
	default:
		assert (0 == 1 && "BAD ARCH");
		exit(1);
	}
	if (k_platform) {
		size_t len = strlen(k_platform) + 1;
		sp.o -= 8;
		mem->memcpy(sp, k_platform, len);
		u_platform = sp;
	}
	
	/* random bytes used for aslr */
	guest_ptr random_bytes(sp.o - 16);
	char random_bytes_data[16];
	int rnd = open("/dev/urandom", O_RDONLY);
	rnd_br = read(rnd, &random_bytes_data[0], 16);
	assert (rnd_br == 16 && "Bad urandom read");
	close(rnd);
	mem->memcpy(random_bytes, &random_bytes_data[0], 16);
	sp = random_bytes;

	ElfTable table;
	/* AT_SYSINFO_EHDR, AT_EXECFD ? */

	switch(img->getArch()) {
	case Arch::ARM:
		/* more? */
		table.add(AT_HWCAP, ARM_HWCAP_THUMB | ARM_HWCAP_NEON | 
			ARM_HWCAP_VFPv3 | ARM_HWCAP_TLS);
		break;
	case Arch::I386:
		/* check me!!! stolen from below*/
		table.add(AT_HWCAP, 0x000000000febfbff);
		break;
	case Arch::X86_64:
		/* less? */
		table.add(AT_HWCAP, 0x000000000febfbff);
		break;
	case Arch::Unknown:
		/* less? */
		table.add(AT_HWCAP, 0);
		break;
	}

	table.add(AT_PAGESZ, img->getPageSize());
	table.add(AT_CLKTCK, sysconf(_SC_CLK_TCK));
	table.add(AT_PHDR, img->getHeader());
	table.add(AT_PHENT, img->getElfPhdrSize());
	table.add(AT_PHNUM, img->getHeaderCount());
	
	table.add(AT_BASE, img->getInterp() ?
		img->getInterp()->getFirstSegment()->relocation()
		: img->getFirstSegment()->relocation());
	table.add(AT_FLAGS, 0);
	table.add(AT_ENTRY, img->getEntryPoint());
	table.add(AT_UID, getuid());
	table.add(AT_EUID, geteuid());
	table.add(AT_GID, getgid());
	table.add(AT_EGID, getegid());
	table.add(AT_SECURE, 0);
	table.add(AT_RANDOM, random_bytes);
	table.add(AT_EXECFN, exe_string);
	table.add(AT_PLATFORM, u_platform);
	table.add(AT_NULL, 0);
	
	/* align to 16 bytes for the entry point */
	int items = argc + 1 + envc + 1 + 1 + table.size() * 2 ;
	int sz = img->getAddressBits() == 32 ? 
		sizeof(int) * items
		: sizeof(long) * items;
	while(((uintptr_t)sp - sz) & 0xf) 
		pushPadByte();
		
	foreach(it, table.rbegin(), table.rend()) {
		pushNative(it->second);
		pushNative(it->first);
	}
	
	loaderBuildArgptr(envc, argc, string_stack, 0);
}

/* Construct the envp and argv tables on the target stack.	*/
void GuestELF::loaderBuildArgptr(int envc, int argc,
	guest_ptr stringp, int push_ptr)
{
	guest_ptr envp;
	guest_ptr argv;

	/* reserve space for the environment string pointers and a null */
	for(int i = 0; i <= envc; ++i) pushPointer(guest_ptr(0));
	envp = sp;

	/* reserve space for the argument string pointers and a null */
	for(int i = 0; i <= argc; ++i) pushPointer(guest_ptr(0));
	argv = sp;

	/* our ABI doesn't need this */
	if (push_ptr) {
		pushPointer(envp);
		pushPointer(argv);
	}
	
	pushNative(argc);
		
	/* copy all the arg pointers into the table */
	while (argc-- > 0) {
		putPointer(argv, stringp, 1);
		stringp.o += mem->strlen(stringp) + 1;
	}

	/* copy all the env pointers into the table */
	while (envc-- > 0) {
		putPointer(envp, stringp, 1);
		stringp.o += mem->strlen(stringp) + 1;
	}
}

void GuestELF::setArgv(unsigned int argc, const char* argv[],
	int envc, const char* envp[])
{
	const char* filename;

	arg_stack = img->getPageSize()*MAX_ARG_PAGES-sizeof(void*);
	filename = getBinaryPath();

	copyElfStrings(1, &filename);
	copyElfStrings(envc, envp);
	if(!img->getLibraryRoot().empty()) {
		ld_library_path = 
			"LD_LIBRARY_PATH=" + 
			img->getLibraryRoot() +
			"/lib";
		const char* extra_env[] = {
			ld_library_path.c_str(),
		};
		copyElfStrings(1, extra_env);
		++envc;
	}
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
			char* buffer = new char[(*it)->length()];
			mem->memcpy(buffer, (*it)->base(), (*it)->length());
			o.write(buffer, (*it)->length());
			delete [] buffer;
		}
		std::ostringstream save_fname;
		save_fname << argv[0] << "." << getpid() 
			<< "." << (void*)stack_limit.o;
		std::ofstream o(save_fname.str().c_str());
		char* buffer = new char[stack_base - stack_limit];
		mem->memcpy(buffer, stack_limit, stack_base - stack_limit);
		o.write(buffer, stack_base - stack_limit);
		delete [] buffer;
	}

	cpu_state->setStackPtr(sp);
	setupMem();
}

void GuestELF::setupMem(void)
{
	/* now this just fills in static pages ... elf segment builds
	   the real guest mappings on the fly via calls to the GuestMem
	   class */

	if(img->getArch() == Arch::ARM) {
		guest_ptr tls;
		int res = mem->mmap(tls, guest_ptr(0xffff0000), PAGE_SIZE, 
			PROT_WRITE | PROT_EXEC | PROT_READ,
			MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
			
		assert(res == 0 && "failed to allocate arm tls");
		/* put the kernel helpers: see arch/arm/kernel/entry-armv.S */
		/* note that this implementation depends on the system 
		   restarting this code block if a context switch happens
		   during its execution */
		/* @0xffff0fc0 cmpxchg
			ldr	r3, [r2]
			subs	r3, r3, r0
			streq	r1, [r2]
			rsbs	r0, r3, #0
			bx	lr
		*/
		mem->write<unsigned int>(guest_ptr(0xffff0fc0), 0xe5923000);
		mem->write<unsigned int>(guest_ptr(0xffff0fc4), 0xe0533000);
		mem->write<unsigned int>(guest_ptr(0xffff0fc8), 0x05821000);
		mem->write<unsigned int>(guest_ptr(0xffff0fcc), 0xe2730000);
		mem->write<unsigned int>(guest_ptr(0xffff0fd0), 0xe12fff1e);
		/* @0xffff0fe0 get_tls
			mrc     p15, 0, r0, c13, c0, 3 
			bx	r14
		*/
		mem->write<unsigned int>(guest_ptr(0xffff0fe0), 0xee1d0f70);
		mem->write<unsigned int>(guest_ptr(0xffff0fe4), 0xe12fff1e);
		/* note: there are others that we may need */
		mem->mprotect(guest_ptr(0xffff0000), PAGE_SIZE, 
			PROT_EXEC | PROT_READ);
	}
}
Arch::Arch GuestELF::getArch() const {
	return img->getArch();
}

void GuestELF::pushPointer(guest_ptr v) {
	pushNative((uintptr_t)v);
}
void GuestELF::pushNative(uintptr_t v) {
	if(img->getAddressBits() == 32) {
		assert(!(v & ~0xFFFFFFFFULL));
		sp.o -= sizeof(unsigned int);
		mem->write<unsigned>(sp, v);
	} else {
		/* only other option is 64-bit and that means
		   we are on a 64-bit host, so smash it in there*/
		sp.o -= sizeof(uintptr_t);
		mem->write<uintptr_t>(sp, v);
	}
}
void GuestELF::putPointer(guest_ptr& p, guest_ptr v, ssize_t inc) {
	putNative(p, (uintptr_t)v, inc);
}
void GuestELF::putNative(guest_ptr& p, uintptr_t v, ssize_t inc) {
	if(img->getAddressBits() == 32) {
		assert(!(v & ~0xFFFFFFFFULL));
		mem->write<unsigned>(p, v);
	} else {
		/* only other option is 64-bit and that means
		   we are on a 64-bit host, so smash it in there*/
		mem->write<uintptr_t>(p, v);
	}
	nextNative(p, inc);
}	
void GuestELF::nextNative(guest_ptr& p, ssize_t num) {
	if(img->getAddressBits() == 32) {
		p.o += num * sizeof(unsigned int);
	} else {
		/* only other option is 64-bit and that means
		   we are on a 64-bit host, so smash it in there*/
		p.o += num * sizeof(uintptr_t);
	}
}

void GuestELF::pushPadByte() {
	sp.o--;
	mem->write<char>(sp, 0);
}
