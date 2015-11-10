#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include "Sugar.h"
#include "cpu/ptimgamd64.h"
#include "cpu/ptamd64cpustate.h"
#include "syscall/syscallsmarshalled.h"

#define _pt_cpu	((PTAMD64CPUState*)pt_cpu.get())

PTImgAMD64::PTImgAMD64(GuestPTImg* gs, int in_pid)
: PTImgArch(gs, in_pid)
{
	if (in_pid)
		pt_cpu = std::make_unique<PTAMD64CPUState>(in_pid);
}

void PTImgAMD64::setPID(int in_pid)
{
	child_pid = in_pid;
	pt_cpu = std::make_unique<PTAMD64CPUState>(in_pid);
}

#define OPCODE_SYSCALL	0x050f

bool PTImgAMD64::isOnSysCall()
{
	long	cur_opcode;
	bool	is_chk_addr_syscall;

	cur_opcode = getInsOp();
	is_chk_addr_syscall = ((cur_opcode & 0xffff) == OPCODE_SYSCALL);
	return is_chk_addr_syscall;
}

bool PTImgAMD64::isOnRDTSC()
{
	long	cur_opcode;
	cur_opcode = getInsOp();
	return (cur_opcode & 0xffff) == 0x310f;
}

bool PTImgAMD64::isOnCPUID()
{
	long	cur_opcode;
	cur_opcode = getInsOp();
	return (cur_opcode & 0xffff) == 0xA20F;
}

bool PTImgAMD64::isPushF()
{
	long	cur_opcode;
	cur_opcode = getInsOp();
	return (cur_opcode & 0xff) == 0x9C;
}

void PTImgAMD64::ignoreSysCall(void)
{
	/* do special syscallhandling if we're on an opcode */
	assert (isOnSysCall());
	_pt_cpu->ignoreSysCall();
	return;
}

bool PTImgAMD64::doStepPreCheck(guest_ptr start, guest_ptr end, bool& hit_syscall)
{
	// XXX try ptimgamd64 version first
	struct user_regs_struct	&regs(_pt_cpu->getRegs());

	/* check rip before executing possibly out of bounds instruction*/
	if (regs.rip < start || regs.rip >= end) {
		if(log_steps) {
			std::cerr << "STOPPING: "
				<< (void*)regs.rip << " not in ["
				<< (void*)start.o << ", "
				<< (void*)end.o << "]"
				<< std::endl;
		}
		/* out of bounds, report back, no more stepping */
		return false;
	}

	/* instruction is in-bounds, run it */
	if (log_steps)
		std::cerr << "STEPPING: " << (void*)regs.rip << std::endl;

	if (isOnSysCall()) {
		/* break on syscall */
		hit_syscall = true;
		return false;
	}

	return true;
}

bool PTImgAMD64::doStep(guest_ptr start, guest_ptr end, bool& hit_syscall)
{
	if (!doStepPreCheck(start, end, hit_syscall))
		return false;

	waitForSingleStep();
	return true;
}

void PTImgAMD64::stepSysCall(SyscallsMarshalled* sc_m)
{
	auto	regs = &_pt_cpu->getRegs();
	long	old_rdi, old_r10;
	bool	syscall_restore_rdi_r10;
	int	sys_nr;

	/* do special syscallhandling if we're on an opcode */
	assert (isOnSysCall());

	syscall_restore_rdi_r10 = false;
	old_rdi = regs->rdi;
	old_r10 = regs->r10;

	if (filterSysCall()) {
		ignoreSysCall();
		return;
	}

	sys_nr = regs->rax;
	if (sys_nr == SYS_mmap || sys_nr == SYS_brk) {
		syscall_restore_rdi_r10 = true;
	}

	waitForSingleStep();

	regs = &_pt_cpu->getRegs();
	if (syscall_restore_rdi_r10) {
		regs->r10 = old_r10;
		regs->rdi = old_rdi;
	}

	//kernel clobbers these, assuming that the generated code, causes
	regs->rcx = regs->r11 = 0;
	_pt_cpu->setRegs(*regs);

	/* fixup any calls that affect memory */
	if (sc_m->isSyscallMarshalled(sys_nr)) {
		SyscallPtrBuf	*spb = sc_m->takePtrBuf();
		_pt_cpu->copyIn(spb->getPtr(), spb->getData(), spb->getLength());
		delete spb;
	}
}

void PTImgAMD64::slurpRegisters(void)
{
	_pt_cpu->loadRegs();

	auto &regs = _pt_cpu->getRegs();
	if (regs.fs_base != 0) {
		return;
	}

	/* if it's static, it'll probably be using
	 * some native register bullshit for the TLS and it'll read 0.
	 *
	 * Patch this transgression up by allocating some pages
	 * to do the work.
	 * (if N/A, the show goes on as normal)
	 */

	/* Yes, I tried ptrace/ARCH_GET_FS. NO DICE. */
	//err = ptrace(
	//	PTRACE_ARCH_PRCTL, pid, &regs.fs_base, ARCH_GET_FS);
	// fprintf(stderr, "%d %p\n",  err, regs.fs_base);

	int		res;
	guest_ptr	base_addr;

	/* I saw some negative offsets when grabbing errno,
	 * so allow for at least 4KB in either direction */
	res = gs->getMem()->mmap(
		base_addr,
		guest_ptr(0),
		4096*2,
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS,
		-1,
		0);
	assert (res == 0);
	regs.fs_base = base_addr.o + 4096;
}

void PTImgAMD64::getRegs(user_regs_struct& r) const
{ memcpy(&r, &_pt_cpu->getRegs(), sizeof(r)); }

void PTImgAMD64::setRegs(const user_regs_struct& r)
{ _pt_cpu->setRegs(r); }

void PTImgAMD64::fixupRegsPreSyscall(void)
{
	int	err;
	uint8_t	*x;
	auto	cpu = gs->getCPUState();

	x = (uint8_t*)gs->getMem()->getHostPtr(cpu->getPC());

	assert ((((x[-1] == 0x05 && x[-2] == 0x0f) /* syscall */ ||
		(x[-2] == 0xcd && x[-1] == 0x80) /* int0x80 */) ||
		(x[-2] == 0xeb && x[-1] == 0xf3)) /* sysenter nop trampoline*/
		&& "not syscall opcode?");

	if (x[-2] == 0xcd || x[-2] == 0xeb) {
		/* XXX:int only used by i386, never amd64? */
		guest_ptr	new_pc;

		assert (gs->getArch() == Arch::I386);

		new_pc = cpu->getPC();
		new_pc.o -= (x[-2] == 0xcd)
			? 2	/* int */
			: 11;	/* sysenter */
		cpu->setPC(new_pc);
	} else {
		/* URK. syscall's RAX isn't stored in RAX! */
		struct user_regs_struct	r;
		err = ptrace(
			__ptrace_request(PTRACE_GETREGS), child_pid, NULL, &r);
		assert (err == 0);
		cpu->setPC(guest_ptr(cpu->getPC()-2));
		gs->setSyscallResult(r.orig_rax);
	}
}
