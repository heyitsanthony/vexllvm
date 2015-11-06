#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>

#include "guestptmem.h"
#include "ptimgarch.h"
#include "ptcpustate.h"
#include "ptctl.h" 

PTCtl::PTCtl(GuestPTImg& _gpi)
: pid(0)
, gpi(_gpi)
, hit_syscall(false)
, bp_steps(0)
{
	log_steps = (getenv("VEXLLVM_LOG_STEPS")) ? true : false;
}

uintptr_t PTCtl::getSysCallResult(void) const
{ return gpi.getPTArch()->getPTCPU().getSysCallResult(); }

void PTCtl::stepSysCall(SyscallsMarshalled* sc_m)
{ gpi.getPTArch()->stepSysCall(sc_m); }

void PTCtl::ignoreSysCall(void) { gpi.getPTArch()->ignoreSysCall(); }
void PTCtl::pushRegisters(void) { gpi.getPTArch()->pushRegisters(); }

guest_ptr PTCtl::stepToBreakpoint(void)
{
	int	err, status;

	bp_steps++;
	gpi.getPTArch()->getPTCPU().revokeRegs();
	err = ptrace(PTRACE_CONT, pid, NULL, NULL);
	if(err < 0) {
		perror("PTCtl::doStep ptrace single step");
		exit(1);
	}
	wait(&status);

	if (!(WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP)) {
		fprintf(stderr,
			"OOPS. status: stopped=%d sig=%d status=%p\n",
			WIFSTOPPED(status), WSTOPSIG(status),
			(void*)(long)status);
		// stackTraceShadow(std::cerr, guest_ptr(0),  guest_ptr(0));
		assert (0 == 1 && "bad wait from breakpoint");
	}

	/* ptrace executes trap, so child process's IP needs to be fixed */
	return gpi.undoBreakpoint();
}


bool PTCtl::breakpointSysCalls(
	const guest_ptr ip_begin,
	const guest_ptr ip_end)
{ return gpi.getPTArch()->breakpointSysCalls(ip_begin, ip_end); }


void PTCtl::pushPage(guest_ptr p) { pushPage(gpi.getMem(), p); }

void PTCtl::pushPage(GuestMem* m, guest_ptr p)
{
	GuestMem::Mapping	mp;
	GuestPTMem		ptmem(&gpi, pid);
	char			buf[4096];

	p.o &= ~0xfffUL;

	if (m->lookupMapping(guest_ptr(p), mp) == false)
		return;

	if (!(mp.getReqProt() & PROT_WRITE))
		return;

	m->memcpy(buf, p, 4096);
	ptmem.memcpy(p, buf, 4096);
}

void PTCtl::stepThroughBounds(guest_ptr start, guest_ptr end)
{
	guest_ptr	new_ip;

	/* TODO: timeout? */
	do {
		new_ip = continueForwardWithBounds(start, end);
		if (hit_syscall)
			break;
	} while (new_ip >= start && new_ip < end);
}

guest_ptr PTCtl::continueForwardWithBounds(guest_ptr start, guest_ptr end)
{
	if (log_steps) {
		std::cerr << "RANGE: "
			<< (void*)start.o << "-" << (void*)end.o
			<< std::endl;
	}

	hit_syscall = false;
	while (gpi.getPTArch()->doStep(start, end, hit_syscall)) {
		guest_ptr	pc(gpi.getPTArch()->getPTCPU().getPC());
		/* so we trap on backjumps */
		if (pc > start)
			start = pc;
	}

	gpi.getPTArch()->incBlocks();
	return gpi.getPTArch()->getPTCPU().getPC();
}
