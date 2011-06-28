#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <list>

#include "Sugar.h"

#include "vexexpr.h"
#include "vexsb.h"
#include "vexxlate.h"

#define VEX_DEBUG_LEVEL	0
#define VEX_TRACE_FLAGS	0

struct vex_cb
{
	uint64_t	cb_guestaddr;
	VexSB*		cb_vexsb;
};

static vex_cb g_cb;
static std::list<std::string>		xlate_msg_log;
static VexXlate::VexXlateLogType	log_type;

#if defined(__amd64__)
/* we actually do have some extensions... */
#define VEX_HOST_HWCAPS	0
#define VEX_HOST_ARCH	VexArchAMD64
#elif defined(__i386__)
/* we actually do have some extensions... */
#define VEX_HOST_HWCAPS	0
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
	g_cb.cb_vexsb = new VexSB(guest_ptr(g_cb.cb_guestaddr), irsb);
	x = 0;
	return irsb;
}
static UInt vex_needs_self_check(void*, VexGuestExtents* ) {
	return 0;
}
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

VexXlate::VexXlate(Arch::Arch in_arch)
{
	LibVEX_default_VexArchInfo(&vai_host);
	vai_host.hwcaps = VEX_HOST_HWCAPS;

	LibVEX_default_VexArchInfo(&vai_guest);
	switch(in_arch) {
	case Arch::X86_64:
		arch = VexArchAMD64;
		break;
	case Arch::ARM:
		arch = VexArchARM;
		vai_guest.hwcaps = 
			VEX_HWCAPS_ARM_NEON | 
			VEX_HWCAPS_ARM_VFP3 | 
			VEX_HWCAPS_ARM_VFP2 | 
			VEX_HWCAPS_ARM_VFP | 
			7;
		break;
	case Arch::I386:
		arch = VexArchX86;
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
}

VexXlate::~VexXlate()
{
	if (getenv("VEXLLVM_DUMP_XLATESTATS")) {
		std::cerr << "VexLate: Dumping op stats\n";
		for (unsigned int i = Iop_INVALID+1; i < Iop_Rsqrte32x4 /* XXX */; ++i) {
			IROp	op = (IROp)i;
			std::cerr <<
				"[VEXLLVM] [VEXXLATESTAT] " <<
				getVexOpName(op) << ": " <<
				VexExpr::getOpCount(op) << std::endl;
		}
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

/* XXX eventually support other architectures */
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
	vta.arch_host = VEX_HOST_ARCH;
	vta.archinfo_host = vai_host;
	vbi.guest_stack_redzone_size = 128;		/* I LOVE RED ZONE. BEST ABI BEST.*/
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
	if(getenv("VEXLLVM_TRACE_FE")) {
		vta.traceflags |= (1 << 7);
	}
#if !defined(VALGRIND_TRUNK)
	vta.dispatch = (void*)dispatch_asm_amd64;
	res = LibVEX_Translate(&vta);
	if (res == VexTransAccessFail) return NULL;
#else
	vta.dispatch_assisted = (void*)dispatch_asm_amd64;
	vta.dispatch_unassisted = (void*)dispatch_asm_amd64;
	vta.irsb_only = true;
	vta.needs_self_check = vex_needs_self_check;
	res = LibVEX_Translate(&vta);
	if (res.status == VexTranslateResult::VexTransAccessFail) return NULL;
#endif

	return g_cb.cb_vexsb;
}
