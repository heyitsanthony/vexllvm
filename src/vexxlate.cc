#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <unistd.h>

#include <string>
#include <list>

#include "Sugar.h"

#include "vexexpr.h"
#include "vexsb.h"
#include "vexxlate.h"
#include "fragcache.h"

#define VEX_DEBUG_LEVEL	0
#define VEX_TRACE_FLAGS	0

struct vex_cb
{
	uint64_t	cb_guestaddr;
	VexSB*		cb_vexsb;
};

static vex_cb 				g_cb;
static jmp_buf				g_env;
static std::list<std::string>		xlate_msg_log;
static VexXlate::VexXlateLogType	log_type;

#ifdef USE_SVN
#define VEX_GUEST_CAPS_X86	VEX_HWCAPS_X86_MMXEXT |	\
				VEX_HWCAPS_X86_SSE1 |	\
				VEX_HWCAPS_X86_SSE2 |	\
				VEX_HWCAPS_X86_SSE3 |	\
				VEX_HWCAPS_X86_LZCNT
#else
#define VEX_GUEST_CAPS_X86	VEX_HWCAPS_X86_SSE1 |	\
				VEX_HWCAPS_X86_SSE2 |	\
				VEX_HWCAPS_X86_SSE3 |	\
				VEX_HWCAPS_X86_LZCNT
#endif

#if defined(__amd64__)
/* we actually do have some extensions... */
#define VEX_HOST_HWCAPS	(VEX_HWCAPS_X86_SSE1 | VEX_HWCAPS_X86_SSE2 | VEX_HWCAPS_X86_SSE3 | VEX_HWCAPS_X86_LZCNT)
#define VEX_HOST_ARCH	VexArchAMD64
#elif defined(__i386__)
/* we actually do have some extensions... */
#define VEX_HOST_HWCAPS	VEX_GUEST_CAPS_X86
#define VEX_HOST_ARCH	VexArchX86
#elif defined(__arm__)
/* pretend we're just arm 7 for now.. */
#define VEX_HOST_HWCAPS 7
#define VEX_HOST_ARCH	VexArchARM
#else
#error Unsupported Host Architecture
#endif

/* built-in handlers */
static __attribute__((noreturn)) void vex_exit(void)
{
	printf("[VEXLLVM] LIBVEX: FAILURE EXIT\n");
	exit(1);
}

static IRSB* vex_finaltidy(IRSB* irsb)
{
	static int	x = 0;

	assert(x == 0 && "NOT REENTRANT RIGHT NOW");
	x = 1;
	g_cb.cb_vexsb = VexSB::create(guest_ptr(g_cb.cb_guestaddr), irsb);
	x = 0;

	/* shoot past vex's "mandatory" assembly generation
	 * no need to worry about leaks-- vex cleans the arena on translate */
	longjmp(g_env, 1);

	return irsb;
}

static UInt vex_needs_self_check(void*, VexGuestExtents* ) { return 0; }

static void vex_log(HChar* hc, Int nbytes)
{
	switch (log_type) {
	case VexXlate::VX_LOG_NONE:
		break;
	case VexXlate::VX_LOG_MEM:
		/* TODO: limit output, merge strings.. */
		xlate_msg_log.push_back(std::string(hc));
		break;
	case VexXlate::VX_LOG_STDERR:
		fprintf(stderr, "%s", hc);
		break;
	}
}

static Bool vex_chase_ok(void* cb, Addr64 x) { return false; }

#define ARM_HWCAPS	\
			VEX_HWCAPS_ARM_NEON |	\
			VEX_HWCAPS_ARM_VFP3 |	\
			VEX_HWCAPS_ARM_VFP2 |	\
			VEX_HWCAPS_ARM_VFP |	\
			7

VexXlate::VexXlate(Arch::Arch in_arch)
: trace_fe(getenv("VEXLLVM_TRACE_FE") != NULL)
, store_fragments(getenv("VEXLLVM_STORE_FRAGS") != NULL)
, frag_cache(NULL)
, frag_log_fd(-1)
{
	LibVEX_default_VexArchInfo(&vai_host);

	vai_host.hwcaps = VEX_HOST_HWCAPS;
	if (in_arch == Arch::ARM)
		vai_host.hwcaps = ARM_HWCAPS;
	else if (in_arch == Arch::MIPS32)
		vai_host.hwcaps = VEX_PRID_COMP_MIPS;

	LibVEX_default_VexArchInfo(&vai_guest);

	switch(in_arch) {
	case Arch::X86_64:
		arch = VexArchAMD64;
		vai_guest.hwcaps |=	VEX_HWCAPS_AMD64_SSE3
				|	VEX_HWCAPS_AMD64_CX16
				|	VEX_HWCAPS_AMD64_LZCNT
				|	VEX_HWCAPS_AMD64_AVX
#ifdef USE_SVN
				|	VEX_HWCAPS_AMD64_AVX2
				|	VEX_HWCAPS_AMD64_BMI
#endif
				;
		vai_host.hwcaps = vai_guest.hwcaps;
		break;
	case Arch::ARM:
		arch = VexArchARM;
		vai_guest.hwcaps = ARM_HWCAPS;
		break;
	case Arch::I386:
		arch = VexArchX86;
		vai_guest.hwcaps |= VEX_GUEST_CAPS_X86;
		break;
	case Arch::MIPS32:
		arch = VexArchMIPS32;
		vai_guest.hwcaps |= VEX_PRID_COMP_MIPS;
		break;
	default:
		assert(!"valid VEX architecture");
	}

	LibVEX_default_VexControl(&vc);
	if(getenv("VEXLLVM_SINGLE_STEP")) {
		vc.guest_max_insns = 1;
		vc.guest_chase_thresh = 0;
	}

	LibVEX_Init(vex_exit, vex_log, VEX_DEBUG_LEVEL, false, &vc);

	LibVEX_default_VexAbiInfo(&vbi);

	loadLogType();

	if (store_fragments) {
		frag_cache = FragCache::create(NULL);
		assert (frag_cache != NULL);
	}

	if (const char* frag_log_fname=getenv("VEXLLVM_FRAG_LOG")) {
		fprintf(stderr,
			"[VEXLLVM] Logging fragments to %s\n",
			frag_log_fname);
		frag_log_fd = open(
			frag_log_fname,
			O_WRONLY | O_CREAT,
			0660);
		lseek(frag_log_fd, 0, SEEK_END);
		assert (frag_log_fd != -1);
	}
}

VexXlate::~VexXlate()
{
	if (frag_cache) delete frag_cache;

	if (frag_log_fd != -1) close(frag_log_fd);

	if (getenv("VEXLLVM_DUMP_XLATESTATS") == NULL)
		return;

	std::cerr << "VexLate: Dumping op stats\n";
	for (unsigned i = Iop_INVALID+1; i < Iop_Rsqrte32x4 /* XXX */; ++i) {
		IROp	op = (IROp)i;
		std::cerr <<
			"[VEXLLVM] [VEXXLATESTAT] " <<
			getVexOpName(op) << ": " <<
			VexExpr::getOpCount(op) << std::endl;
	}
}

void VexXlate::loadLogType(void)
{
	const char*	s;

	s = getenv("VEXLLVM_SB_LOG");
	if (s == NULL) log_type = VX_LOG_NONE;
	else if (strcmp(s, "mem") == 0) log_type = VX_LOG_MEM;
	else if (strcmp(s, "stderr") == 0) log_type = VX_LOG_STDERR;
	else assert (0 == 1 && "bad log type. expect from {mem,stderr}");
}

void VexXlate::dumpLog(std::ostream& os) const
{
	foreach (it, xlate_msg_log.begin(), xlate_msg_log.end()) {
		os << (*it);
	}
	os << std::endl;
}

VexSB* VexXlate::patchBadDecode(const void* guest_bytes, uint64_t guest_addr)
{
	const unsigned char *gb = (const unsigned char*)(guest_bytes);

	if (arch == VexArchX86) {
		for (unsigned i = 0; i < 128; i++) {
			/* windows syscall patch (int 0x23)
			 * -- pretend it's a linux syscall at int 0x80 */
			if (gb[i] == 0xcd && gb[i+1] == 0x2e) {
				char		dumb_buf[128];
				memcpy(dumb_buf, gb, 128);
				printf("[VexXlate] PATCH UP@%p\n",
					(void*)(guest_addr+i));
				dumb_buf[i+1] = 0x80;
				return xlate(dumb_buf, guest_addr);
			}
		}
	}

	/* XXX: add 32-bit syscall patch */

	return NULL;
}


VexSB* VexXlate::xlate(const void* guest_bytes, uint64_t guest_addr)
{
	VexTranslateArgs	vta;
	VexGuestExtents		vge;
	VexTranslateResult	res;
	Int			host_bytes_used;	/* WHY A PTR?? */
	uint8_t			b[1024 /* bogus buffer for VX->MC dump */];

	memset(&vta, 0, sizeof(vta));
	vta.arch_guest = arch;
	vta.archinfo_guest = vai_guest;

	//vta.arch_host = VEX_HOST_ARCH;
	// this is just lipservice -- we longjmp out of host iselection
	vta.arch_host = arch;

	vta.archinfo_host = vai_host;
	if (arch == VexArchX86) {
		/* vex yelled at me to zero this */
		vbi.guest_stack_redzone_size = 0;
		vta.archinfo_host.hwcaps = vai_guest.hwcaps;
	} else {
		/* amd64 reserves 128 bytes below the stack frame
		 * for turbo-nerd functions like memcpy. kind of breaks things */
		vbi.guest_stack_redzone_size = 128;
	}
	vbi.guest_amd64_assume_fs_is_zero = true;	/* XXX LIBVEX FIXME */
	vta.abiinfo_both = vbi;

	g_cb.cb_guestaddr = guest_addr;
	vta.callback_opaque = NULL;	/* no local callback */

	vta.guest_bytes = static_cast<uint8_t*>(
		const_cast<void*>(guest_bytes));	/* so stupid */
	vta.guest_bytes_addr = guest_addr; /* where guest thinks it is */

	vta.chase_into_ok = vex_chase_ok;	/* not OK */
	vta.guest_extents = &vge;
	vta.finaltidy = vex_finaltidy;

	/* we're not decoding anyway, but libVex doesn't let us
	 * total skip it. Joy */
	vta.host_bytes = b;
	vta.host_bytes_size = 0;
	vta.host_bytes_used = &host_bytes_used;

	vta.traceflags = VEX_TRACE_FLAGS;
	if (trace_fe) vta.traceflags |= (1 << 7);

	/* lipservice -- should never be used */
	vta.disp_cp_chain_me_to_slowEP = NULL;
	vta.disp_cp_xassisted = (void*)0xdeadbeef;

	vta.needs_self_check = vex_needs_self_check;

	if (setjmp(g_env) == 0) {
		res = LibVEX_Translate(&vta);
	} else {
		/* parachuted out of finaltidy */
		res.status = VexTranslateResult::VexTransOK;
	}

	if (res.status == VexTranslateResult::VexTransAccessFail)
		return NULL;

	if (g_cb.cb_vexsb == NULL)
		return patchBadDecode(guest_bytes, guest_addr);

	if (g_cb.cb_vexsb->getSize()) {
		if (frag_cache)
			frag_cache->addFragment(
				guest_bytes, g_cb.cb_vexsb->getSize());
		if (frag_log_fd != -1) {
			ssize_t	bw;

			bw = write(
				frag_log_fd,
				guest_bytes,
				g_cb.cb_vexsb->getSize());
			(void)bw;
			/* bw == -1 when we run the fucking syscall page */
		}
	}

	return g_cb.cb_vexsb;
}
