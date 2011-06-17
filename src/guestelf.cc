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
#define elf_addr_t	Elf64_Addr
#define ELF_PLATFORM	"x86_64"
#define elf_phdr	Elf64_Phdr
#define ELF_HWCAP	0x000000000febfbff
#define target_mmap(a, b, c, d, e, f) \
	(guestptr_t)mmap((void*)(a), b, c, d, e, f)
#define target_mprotect(a, b, c) (guestptr_t)mprotect((void*)(a), b, c)
#define memcpy_to_target(a, b, c) memcpy((void*)a, b, c)
#define put_user_ual(v,a)	*(guestptr_t*)(a) = v;
#define memcpy_fromfs	memcpy
#define target_strlen(a) strlen((const char*)(a))


GuestELF::GuestELF(ElfImg* in_img)
: Guest(in_img->getFilePath())
, img(in_img)
, arg_pages(MAX_ARG_PAGES)
{
	stack = new uint8_t[STACK_BYTES];
	memset(stack, 0x32, STACK_BYTES);	/* bogus data */
	cpu_state->setStackPtr(stack + STACK_BYTES-256 /*redzone+gunk*/);
}

GuestELF::~GuestELF(void) { delete [] stack; }

Value* GuestELF::addrVal2Host(Value* addr_v) const
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
	assert (0 == 1 && "SLOW PATH LOAD NOT IMPLEMENTED");
//	addr_v = new  builder->CreateCall(f, , "__addr2Host");
	addr_v->dump();

	return addr_v;
}

uintptr_t GuestELF::addr2Host(uintptr_t guestptr) const
{
	return (uintptr_t)img->xlateAddr((elfptr_t)guestptr);
}

void* GuestELF::getEntryPoint(void) const { 
	if(img->getInterp())
		return img->getInterp()->getFirstSegment()->
			xlate(img->getInterp()->getEntryPoint()); 
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

#define TARGET_PAGE_SIZE 4096
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
				offset = arg_stack % TARGET_PAGE_SIZE;
				pag = (char *)arg_pages[arg_stack/
					TARGET_PAGE_SIZE];
				if (!pag) {
					pag = (char *)
						malloc(TARGET_PAGE_SIZE);
					memset(pag, 0, TARGET_PAGE_SIZE);
					arg_pages[arg_stack/
						TARGET_PAGE_SIZE] = pag;
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
				memcpy_fromfs(pag + offset, tmp, 
					bytes_to_copy + 1);
			}
		}
	}
}

//borrowed liberally from qemu
void GuestELF::setupArgPages()
{
	guestptr_t size, error, guard;

	/* Create enough stack to hold everything.	If we don't use
	it for args, we'll use it for something else.  */
	size = STACK_BYTES;
	if (size < MAX_ARG_PAGES*TARGET_PAGE_SIZE) {
		size = MAX_ARG_PAGES*TARGET_PAGE_SIZE;
	}
	/* TODO: this check and math really isn't right */
	guard = TARGET_PAGE_SIZE;
	if (guard < PAGE_SIZE) {
		guard = PAGE_SIZE;
	}

	error = target_mmap(0, size + guard, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if ((int64_t)error == -1) {
		perror("mmap stack");
		exit(-1);
	}

	/* We reserve one extra page at the top of the stack as guard.	*/
	target_mprotect(error, guard, PROT_NONE);

	stack_limit = error + guard;
	stack_base = stack_limit + size -
		MAX_ARG_PAGES*TARGET_PAGE_SIZE;
	arg_stack += stack_base;

	foreach(it, arg_pages.begin(), arg_pages.end()) {
		if (*it) {
			// info->rss++;
			/* FIXME - check return value of 
			   memcpy_to_target() for failure */
			memcpy_to_target(stack_base, *it, TARGET_PAGE_SIZE);
			free(*it);
			*it = NULL;
		}
		stack_base += TARGET_PAGE_SIZE;
	}
}

void GuestELF::createElfTables(int argc, int envc)
{
	guestptr_t string_stack = arg_stack;
	guestptr_t& sp = arg_stack;
	int size;
	guestptr_t u_platform;
	const char *k_platform;
	const int n = sizeof(elf_addr_t);

#ifdef CONFIG_USE_FDPIC
	/* Needs to be before we load the env/argc/... */
	if (elf_is_fdpic(exec)) {
		/* Need 4 byte alignment for these structs */
		sp &= ~3;
		sp = loader_build_fdpic_loadmap(info, sp);
		info->other_info = interp_info;
		if (interp_info) {
			interp_info->other_info = info;
			sp = loader_build_fdpic_loadmap(interp_info, sp);
		}
	}
#endif

	u_platform = 0;
	k_platform = ELF_PLATFORM;
	if (k_platform) {
		size_t len = strlen(k_platform) + 1;
		sp -= (len + n - 1) & ~(n - 1);
		u_platform = sp;
		/* FIXME - check return value of memcpy_to_target() for failure */
		memcpy_to_target(sp, k_platform, len);
	}
	/* random bytes used for aslr */
	guestptr_t random_bytes = sp - 16;
	int rnd = open("/dev/urandom", O_RDONLY);
	read(rnd, (void*)random_bytes, 16);
	close(rnd);
	sp -= 16;

#define DLINFO_ITEMS 14
	/*
	 * Force 16 byte _final_ alignment here for generality.
	 */
	sp = sp &~ (guestptr_t)16;
	size = (DLINFO_ITEMS + 1) * 2;
	if (k_platform)
		size += 2;
#ifdef DLINFO_ARCH_ITEMS
	size += DLINFO_ARCH_ITEMS * 2;
#endif
	size += envc + argc + 2;
	size += 1;	/* argc itself */
	size *= n;
	if (size & 15)
		sp -= 16 - (size & 15);

	/* This is correct because Linux defines
	 * elf_addr_t as Elf32_Off / Elf64_Off
	 */
#define NEW_AUX_ENT(id, val) do {				\
		sp -= n; put_user_ual(val, sp);			\
		sp -= n; put_user_ual(id, sp);			\
	} while(0)

	NEW_AUX_ENT (AT_NULL, 0);


	/* There must be exactly DLINFO_ITEMS entries here.	 */
	if (k_platform)
		NEW_AUX_ENT(AT_PLATFORM, u_platform);
	//ptr to filename
	NEW_AUX_ENT(AT_EXECFN, (guestptr_t)exe_string);

	NEW_AUX_ENT(AT_RANDOM, random_bytes);
	NEW_AUX_ENT(AT_SECURE, (guestptr_t)0); //not suid

	NEW_AUX_ENT(AT_EGID, (guestptr_t) getegid());
	NEW_AUX_ENT(AT_GID, (guestptr_t) getgid());
	NEW_AUX_ENT(AT_EUID, (guestptr_t) geteuid());
	NEW_AUX_ENT(AT_UID, (guestptr_t) getuid());
	NEW_AUX_ENT(AT_ENTRY, (guestptr_t)img->getEntryPoint());
	NEW_AUX_ENT(AT_FLAGS, (guestptr_t)0);
	NEW_AUX_ENT(AT_BASE, (guestptr_t)(img->getInterp() ?
	img->getInterp()->getFirstSegment()->relocation()
	: img->getFirstSegment()->relocation()));
	NEW_AUX_ENT(AT_PHNUM, (guestptr_t)(img->getHeaderCount()));
	NEW_AUX_ENT(AT_PHENT, (guestptr_t)(sizeof (elf_phdr)));
	NEW_AUX_ENT(AT_PHDR, (guestptr_t)(img->getHeader()));
	NEW_AUX_ENT(AT_CLKTCK, (guestptr_t) sysconf(_SC_CLK_TCK));
	NEW_AUX_ENT(AT_PAGESZ, (guestptr_t)(TARGET_PAGE_SIZE));
	NEW_AUX_ENT(AT_HWCAP, (guestptr_t) ELF_HWCAP);
	//ptr to syscall - need to fetch, or stick a dummy one in?
	//NEW_AUX_ENT(AT_SYSINFO_EHDR, (guestptr_t)0xffffffffff600000ULL); 
	// //fd for exe
	// NEW_AUX_ENT(AT_EXECFD, (guestptr_t)0xbbbbbbbbbbbbbbbbbULL);

#ifdef ARCH_DLINFO
	/*
	 * ARCH_DLINFO must come last so platform specific code can enforce
	 * special alignment requirements on the AUXV if necessary (eg. PPC).
	 */
	ARCH_DLINFO;
#endif
#undef NEW_AUX_ENT

	/* for completeness we could do something with the note... */
	// info->saved_auxv = sp;

	loaderBuildArgptr(envc, argc, string_stack, 0);
}

/* Construct the envp and argv tables on the target stack.	*/
void GuestELF::loaderBuildArgptr(int envc, int argc,
	guestptr_t stringp, int push_ptr)
{
	guestptr_t& sp = arg_stack;

	int n = sizeof(guestptr_t);
	guestptr_t envp;
	guestptr_t argv;

	sp -= (envc + 1) * n;
	envp = sp;
	sp -= (argc + 1) * n;
	argv = sp;
	if (push_ptr) {
		/* FIXME - handle put_user() failures */
		sp -= n;
		put_user_ual(envp, sp);
		sp -= n;
		put_user_ual(argv, sp);
	}
	sp -= n;
	/* FIXME - handle put_user() failures */
	put_user_ual(argc, sp);
	
	//ts->info->arg_start = stringp;

	exe_string = stringp;
	while (argc-- > 0) {
		/* FIXME - handle put_user() failures */
		put_user_ual(stringp, argv);
		argv += n;
		stringp += target_strlen(stringp) + 1;
	}

	// ts->info->arg_end = stringp;

	/* FIXME - handle put_user() failures */
	put_user_ual(0, argv);
	while (envc-- > 0) {
		/* FIXME - handle put_user() failures */
		put_user_ual(stringp, envp);
		envp += n;
		stringp += target_strlen(stringp) + 1;
	}
	/* FIXME - handle put_user() failures */
	put_user_ual(0, envp);
}

void GuestELF::setArgv(unsigned int argc, const char* argv[],
	int envc, const char* envp[])
{
	const char* filename;

	arg_stack = TARGET_PAGE_SIZE*MAX_ARG_PAGES-sizeof(void*);
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
	}

	cpu_state->setStackPtr((void*)arg_stack);
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
