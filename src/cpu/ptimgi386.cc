#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <asm/ptrace-abi.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "Sugar.h"
#include "cpu/ptimgi386.h"
#include "cpu/pti386cpustate.h"

#define WARNSTR "[PTimgI386] Warning: "
#define INFOSTR "[PTimgI386] Info: "

#define _pt_cpu	((PTI386CPUState*)pt_cpu.get())

PTImgI386::PTImgI386(GuestPTImg* gs, int in_pid)
: PTImgArch(gs, in_pid)
{
	if (in_pid != 0)
		pt_cpu = std::make_unique<PTI386CPUState>(in_pid);
}

PTImgI386::~PTImgI386() {}

void PTImgI386::setPID(int in_pid)
{
	child_pid = in_pid;
	pt_cpu = std::make_unique<PTI386CPUState>(in_pid);
}

bool PTImgI386::doStep(guest_ptr start, guest_ptr end, bool& hit_syscall)
{ assert (0 == 1 && "STUB"); }

void PTImgI386::slurpRegisters(void)
{
	_pt_cpu->getRegs();
	fprintf(stderr, WARNSTR "eflags not computed\n");
	fprintf(stderr, WARNSTR "FP, TLS, etc not supported\n");
}

void PTImgI386::setFakeInfo(const char* info_file)
{
	FILE*		f;
	uint32_t	cur_off;

	f = fopen(info_file, "rb");
	assert (f != NULL && "could not open patch file");

	while (fscanf(f, "%x\n", &cur_off) > 0)
		patch_offsets.insert(cur_off);

	assert (patch_offsets.size() > 0 && "No patch offsets?");

	fprintf(stderr,
		INFOSTR "loaded %d CPUID patch offsets\n",
		(int)patch_offsets.size());

	fclose(f);
}

#define IS_PEEKTXT_CPUID(x)	(((x) & 0xffff) == 0xa20f)
#define IS_PEEKTXT_INT3(x)	(((x) & 0xff) == 0xcc)

uint64_t PTImgI386::checkCPUID(void)
{
	struct user_regs_struct regs;
	int			err;
	uint64_t		v;

	err = ptrace((__ptrace_request)PTRACE_GETREGS, child_pid, NULL, &regs);
	assert(err != -1);

	v = ptrace(PTRACE_PEEKTEXT, child_pid, regs.rip, NULL);

	if (IS_PEEKTXT_INT3(v))
		return 1;

	if (IS_PEEKTXT_CPUID(v))
		return regs.rip;

	return 0;
}

/* patch all cpuid instructions */
bool PTImgI386::stepInitFixup(void)
{
	int		err, status, prefix_ins_c = 0;
	uint64_t	cpuid_pc = 0, bias = 0;

	/* single step until cpuid instruction */
	while ((cpuid_pc = checkCPUID()) == 0) {
		int	ok;

		err = ptrace(PTRACE_SINGLESTEP, child_pid, NULL, NULL);
		assert (err != -1 && "Bad PTRACE_SINGLESTEP");
		wait(&status);

		ok = (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP);
		if (!ok) {
			if (	WIFSTOPPED(status) &&
				WSTOPSIG(status) == SIGSEGV)
			{
				fprintf(stderr,
					"sigsegv. prefix_ins_c=%d\n",
					prefix_ins_c);
			}

			fprintf(stderr,
				"[PTImgI386] mixed signals! status=%x\n",
				status);
			return false;
		}

		prefix_ins_c++;
	}

	/* int3 => terminate early-- can't trust more code */
	if (cpuid_pc == 1) {
		struct user_regs_struct regs;
		fprintf(stderr, INFOSTR "skipping CPUID patching\n");
		ptrace((__ptrace_request)PTRACE_GETREGS, child_pid, NULL, &regs);
		/* undoBreakpoint code expects the instruction to already
		 * be dispatched, so fake it by bumping the PC */
		regs.rip++; 
		ptrace((__ptrace_request)PTRACE_SETREGS, child_pid, NULL, &regs);
		return true;
	}

	/* find bias of cpuid instruction w/r/t patch offsets */
	foreach (it, patch_offsets.begin(), patch_offsets.end()) {
		/* XXX: "research quality" */
		if ((*it & 0xfff) == (cpuid_pc & 0xfff)) {
			bias = cpuid_pc - *it;
			break;
		}
	}

	fprintf(stderr, INFOSTR "CPUID patch bias %p\n", (void*)bias);

	assert (bias != 0 && "Expected non-zero bias. Bad patch file?");

	/* now have offset of first CPUID instruction.
	 * replace all CPUID instructions with trap instruction. */
	trapCPUIDs(bias);

	/* trap on replaced cpuid instructions until non-cpuid instruction */
	while (patchCPUID() == true) {
		err = ptrace(PTRACE_CONT, child_pid, NULL, NULL);
		assert (err != -1 && "bad ptrace_cont");
		wait(&status);
		assert (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP);
	}

	/* child process should be on a legitimate int3 instruction now */

	/* restore CPUID instructions */
	for (const auto& p : cpuid_insts) {
		_pt_cpu->resetBreakpoint(guest_ptr(p.first), p.second);
	}

	return true;
}
