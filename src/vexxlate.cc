#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <list>

#include "Sugar.h"

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
static std::list<std::string>	xlate_msg_log;

/* built-in handlers */
static __attribute__((noreturn)) void vex_exit(void)
{
	printf("FAILURE EXIT\n");
	exit(1);
}

static IRSB* vex_finaltidy(IRSB* irsb)
{
	static int	x = 0;

	assert(x == 0 && "NOT REENTRANT RIGHT NOW");
	x = 1;
	g_cb.cb_vexsb = new VexSB(g_cb.cb_guestaddr, irsb);
	x = 0;
	return irsb;
}

static void vex_log(HChar* hc, Int nbytes)
#if 0
{
	/* TODO: limit output */
	xlate_msg_log.push_back(std::string(hc));
}
#elif 0
{
	/* TODO: limit output */
	fprintf(stderr, "%s", hc);
}
#else
{}
#endif

static Bool vex_chase_ok(void* cb, Addr64 x) { return false; }

VexXlate::VexXlate()
{
	LibVEX_default_VexControl(&vc);
	LibVEX_Init(vex_exit, vex_log, VEX_DEBUG_LEVEL, false, &vc);

	LibVEX_default_VexArchInfo(&vai_amd64);
	vai_amd64.hwcaps = 0;
	LibVEX_default_VexAbiInfo(&vbi);
}

VexXlate::~VexXlate() { /* ??? */ }

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
	uint8_t			b[1024];

	memset(&vta, 0, sizeof(vta));
	vta.arch_guest = VexArchAMD64;
	vta.archinfo_guest = vai_amd64;
	vta.arch_host = VexArchAMD64;
	vta.archinfo_host = vai_amd64;
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
	vta.dispatch = (void*)dispatch_asm_amd64;

	res = LibVEX_Translate(&vta);
	if (res == VexTransAccessFail) return NULL;

	return g_cb.cb_vexsb;
}
