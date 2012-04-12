#include <sys/types.h>
#include <sys/wait.h>
#include <stddef.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sstream>
#include <sys/syscall.h>
#include <sys/time.h>
#include <set>
#include "Sugar.h"

#include "util.h"
#include "symbols.h"
#include "elfimg.h"
#include "elfdebug.h"
#include "ptimgchk.h"
#include "guestptimg.h"
#include "guestcpustate.h"

#if defined(__amd64__)
#include <asm/ptrace-abi.h>
#include <sys/prctl.h>
#include "cpu/amd64cpustate.h"
#include "cpu/ptimgamd64.h"
#include "cpu/ptimgi386.h"
#endif

#ifdef __arm__
#include <linux/prctl.h>
#include <sys/prctl.h>
#include "cpu/ptimgarm.h"
#endif

using namespace llvm;

GuestPTImg::GuestPTImg(
	int argc, char *const argv[], char *const envp[],
	bool use_entry)
: Guest(argv[0])
, pt_arch(NULL)
, symbols(NULL)
, dyn_symbols(NULL)
, arch(Arch::getHostArch())
{
	mem = new GuestMem();
	ProcMap::dump_maps = (getenv("VEXLLVM_DUMP_MAPS")) ? true : false;

	entry_pt = guest_ptr(0);
	if (argv[0] != NULL && use_entry)  {
		ElfImg	*img = ElfImg::create(argv[0], false);
		assert (img != NULL && "DOES BINARY EXIST?");
		assert(img->getArch() == getArch());

		entry_pt = img->getEntryPoint();
		delete img;
	}

#if defined(__amd64__)
	if (getenv("VEXLLVM_32_ARCH")) {
		arch = Arch::I386;
		mem->mark32Bit();
	}
#endif

	cpu_state = GuestCPUState::create(getArch());
}

GuestPTImg::~GuestPTImg(void)
{
	if (pt_arch != NULL) delete pt_arch;
	if (symbols != NULL) delete symbols;
	if (dyn_symbols != NULL) delete dyn_symbols;
}

void GuestPTImg::handleChild(pid_t pid)
{
	ptrace(PTRACE_KILL, pid, NULL, NULL);
	wait(NULL);
}


#if defined(__amd64__)
#define NEW_ARCH	\
	(!getenv("VEXLLVM_32_ARCH"))	\
	? (PTImgArch*)(new PTImgAMD64(this, pid))	\
	: (PTImgArch*)(new PTImgI386(this, pid))
#elif defined(__arm__)
#define NEW_ARCH	new PTImgARM(this, pid);
#else
#define NEW_ARCH	0; assert (0 == 1 && "UNKNOWN PTRACE HOST ARCHITECTURE! AIEE");
#endif


void GuestPTImg::attachSyscall(int pid)
{
	int	err, status;

	err = ptrace(PTRACE_SYSCALL, pid, 0, NULL, NULL);
	wait(&status);
	assert (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP);
//	fprintf(stderr, "Got syscall from PID=%d\n", pid);
//
#if defined(__amd64__)

	/* slurp brains after trap code is removed so that we don't
	 * copy the trap code into the parent process */
	slurpBrains(pid);

	/* URK. syscall's RAX isn't stored in RAX!?? */
	uint8_t* x = (uint8_t*)getMem()->getHostPtr(getCPUState()->getPC());

	assert ((((x[-1] == 0x05 && x[-2] == 0x0f) /* syscall */ ||
		(x[-2] == 0xcd && x[-1] == 0x80) /* int0x80 */) ||
		(x[-2] == 0xeb && x[-1] == 0xf3)) /* sysenter nop trampoline*/
		&& "not syscall opcode?");

	if (x[-2] == 0xcd || x[-2] == 0xeb) {
		/* XXX:int only used by i386, never amd64? */
		assert (arch == Arch::I386);

		if (x[-2] == 0xcd) {
			getCPUState()->setPC(guest_ptr(getCPUState()->getPC()-2));
		} else {
			/* backup to sysenter */
			getCPUState()->setPC(guest_ptr(getCPUState()->getPC()-11));
		}
	} else {
		struct user_regs_struct	r;
		err = ptrace(__ptrace_request(PTRACE_GETREGS), pid, NULL, &r);
		assert (err == 0);

		getCPUState()->setPC(guest_ptr(getCPUState()->getPC()-2));
		getCPUState()->setSyscallResult(r.orig_rax);
	}
#else
	assert (0 == 1 && "CAN'T HANDLE THIS");
#endif
}

pid_t GuestPTImg::createSlurpedAttach(int pid)
{
	int	err, status;

	// assert (entry_pt.o == 0 && "Only support attaching immediately");
	fprintf(stderr, "Attaching to PID=%d\n", pid);

	pt_arch = NEW_ARCH;

	err = ptrace(PTRACE_ATTACH, pid, 0, NULL, NULL);
	assert (err != -1 && "Couldn't attach to process");

	wait(&status);
	fprintf(stderr, "ptrace status=%p\n", (void*)status);
	assert (WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP);
	fprintf(stderr, "Attached to PID=%d\n", pid);
	
	if (getenv("VEXLLVM_NOSYSCALL") == NULL)
		attachSyscall(pid);
	else
		slurpBrains(pid);

	entry_pt = getCPUState()->getPC();

	/* release the process */
	ptrace(PTRACE_DETACH, pid, 0, NULL, NULL);

	return pid;
}

pid_t GuestPTImg::createSlurpedChild(
	int argc, char *const argv[], char *const envp[])
{
	int			err, status;
	pid_t			pid;
	guest_ptr		break_addr;

	assert (entry_pt.o && "No entry point given to slurp");

	pid = fork();
	if (pid == 0) {
		/* child */
		int     err;

		/* don't keep running if parent dies */
                prctl(PR_SET_PDEATHSIG, SIGKILL);
                prctl(PR_SET_PDEATHSIG, SIGSEGV);
                prctl(PR_SET_PDEATHSIG, SIGILL);
                prctl(PR_SET_PDEATHSIG, SIGBUS);
                prctl(PR_SET_PDEATHSIG, SIGABRT);
		ptrace(PTRACE_TRACEME, 0, NULL, NULL);
		if (getenv("VEXLLVM_PRELOAD")) {
			setenv("LD_PRELOAD", getenv("VEXLLVM_PRELOAD"), 1);
			err = execvp(argv[0], argv);
		} else
			err = execvpe(argv[0], argv, envp);
		assert (err != -1 && "EXECVE FAILED. NO PTIMG!");
	}

	/* failed to create child */
	if (pid < 0) return pid;

	pt_arch = NEW_ARCH;

	/* wait for child to call execve and send us a trap signal */
	wait(&status);
	assert (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP);

	/* Trapped the process on execve-- binary is loaded, but not linked */
	/* overwrite entry with BP. */
	setBreakpoint(pid, entry_pt);

	/* go until child hits entry point */
	err = ptrace(PTRACE_CONT, pid, NULL, NULL);
	assert (err != -1);
	wait(&status);
	assert (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP);

	break_addr = undoBreakpoint(pid);
	assert (break_addr == entry_pt && "Did not break at entry");

	/* hit the entry point, everything should be linked now-- load it
	 * into our context! */
	/* cleanup bp */
	resetBreakpoint(pid, entry_pt);

	if (ProcMap::dump_maps) dumpSelfMap();

	/* slurp brains after trap code is removed so that we don't
	 * copy the trap code into the parent process */
	slurpBrains(pid);

#if (defined(__amd64__) || defined(__arm__))
	/* XXX there should be a better place for this */
	/* from glibc, sysdeps/x86_64/elf/start.S
	 * argc = rsi
	 * argv = rdx */

	/* from glibc-ports
	 * 0(sp)	argc
	 * 4(sp)	argv[0]
	 * ...
	 * (4*argc)(sp)	NULL
	 */
	guest_ptr		in_argv;

	argv_ptrs.clear();
	argc = mem->readNative(pt_arch->getStackPtr());
	in_argv = guest_ptr(mem->readNative(pt_arch->getStackPtr(), 1));
	for (int i = 0; i < argc; i++) {
		argv_ptrs.push_back(in_argv);
		in_argv.o += mem->strlen(in_argv) + 1;
	}
#else
	fprintf(stderr,
		"[VEXLLVM] WARNING: can not get argv for current arch\n");
#endif


	return pid;
}

void GuestPTImg::dumpSelfMap(void)
{
	FILE	*f_self;
	char	buf[256];

	f_self = fopen("/proc/self/maps", "r");
	while (!feof(f_self)) {
		if (fgets(buf, 256, f_self) == NULL)
			break;
		fprintf(stderr, "[ME] %s", buf);
	}
	fclose(f_self);
}

void GuestPTImg::slurpRegisters(pid_t pid)
{
	pt_arch->slurpRegisters();
}

void GuestPTImg::slurpBrains(pid_t pid)
{
	ProcMap::slurpMappings(pid, mem, mappings);
	slurpRegisters(pid);
}

void GuestPTImg::stackTrace(
	std::ostream& os,
	const char* binname, pid_t pid,
	guest_ptr range_begin, guest_ptr range_end)
{
	char			buffer[1024];
	int			bytes;
	int			pipefd[2];
	int			err;
	std::ostringstream	pid_str, disasm_cmd;

	pid_str << pid;
	if (range_begin && range_end) {
		disasm_cmd << "disass " << range_begin << "," << range_end;
	} else {
		disasm_cmd << "print \"???\"";
	}

	err = pipe(pipefd);
	assert (err != -1 && "Bad pipe for subservient trace");

	kill(pid, SIGSTOP);
	ptrace(PTRACE_DETACH, pid, NULL, NULL);

	if (!fork()) {
		close(pipefd[0]);    // close reading end in the child
		dup2(pipefd[1], 1);  // send stdout to the pipe
		dup2(pipefd[1], 2);  // send stderr to the pipe
		close(pipefd[1]);    // this descriptor is no longer needed

		execl("/usr/bin/gdb",
			"/usr/bin/gdb",
			"--batch",
			binname,
			pid_str.str().c_str(),
			"--eval-command",
			"thread apply all bt",
			"--eval-command",
			"print \"disasm of where guest ended (may fail)\"",
			"--eval-command",
			"disass",
			 "--eval-command",
			"print \"disasm of block with error (just finished)\"",
			"--eval-command",
			disasm_cmd.str().c_str(),
			"--eval-command",
			"info registers all",
			"--eval-command",
			"kill",
			(char*)NULL);
		exit(1);
	}

	  // close the write end of the pipe in the parent
	close(pipefd[1]);

	while ((bytes = read(pipefd[0], buffer, sizeof(buffer))) != 0)
		os.write(buffer, bytes);
}

void GuestPTImg::setBreakpoint(pid_t pid, guest_ptr addr)
{
	if (breakpoints.count(addr))
		return;

	breakpoints[addr] = pt_arch->setBreakpoint(addr);
}


guest_ptr GuestPTImg::undoBreakpoint(pid_t pid)
{ return pt_arch->undoBreakpoint(); }

void GuestPTImg::resetBreakpoint(pid_t pid, guest_ptr addr)
{
	uint64_t	old_v;

	assert (breakpoints.count(addr) && "Resetting non-BP!");

	old_v = breakpoints[addr];
	pt_arch->resetBreakpoint(addr, old_v);
	breakpoints.erase(addr);
}

const Symbols* GuestPTImg::getSymbols(void) const
{
	if (!symbols) symbols = loadSymbols(mappings);
	return symbols;
}

const Symbols* GuestPTImg::getDynSymbols(void) const
{
	if (!dyn_symbols)
		dyn_symbols = loadDynSymbols(mem, getBinaryPath());
	return dyn_symbols;
}

Symbols* GuestPTImg::loadSymbols(
	const PtrList<ProcMap>& mappings)
{
	Symbols			*symbols;
	std::set<std::string>	mmap_fnames;
	const char		*preload_lib;

	symbols = new Symbols();

	/* This is a really shitty hack to force VEXLLVM_PRELOAD libraries
	 * to override default symbols. In the future, we need to support
	 * duplicate symbols */
	preload_lib = getenv("VEXLLVM_PRELOAD");
	if (preload_lib != NULL) {
		Symbols		*new_syms;
		guest_ptr	base(0);

		foreach (it, mappings.begin(), mappings.end()) {
			if ((*it)->getLib() == preload_lib) {
				base = (*it)->getBase();
				break;
			}
		}

		assert (base.o && "Could not find VEXLLVM_PRELOAD library!");
		new_syms = ElfDebug::getSyms(preload_lib, base);
		if (new_syms != NULL) {
			symbols->addSyms(new_syms);
			delete new_syms;
		}

		mmap_fnames.insert(preload_lib);
	}

	foreach (it, mappings.begin(), mappings.end()) {
		std::string	libname((*it)->getLib());
		guest_ptr	base((*it)->getBase());
		Symbols		*new_syms;

		if (libname.size() == 0)
			continue;

		/* already seen it? */
		if (mmap_fnames.count(libname) != 0)
			continue;

		/* new fname, try to load the symbols */
		new_syms = ElfDebug::getSyms(libname.c_str(), base);
		if (new_syms) {
			symbols->addSyms(new_syms);
			delete new_syms;
		}

		mmap_fnames.insert(libname);
	}

	return symbols;
}

Symbols* GuestPTImg::loadDynSymbols(
	GuestMem	*mem,
	const char	*binpath)
{
	Symbols	*dyn_syms;
	Symbols	*exec_syms;

	dyn_syms = new Symbols();

	/* XXX, in the future, we should look at what is in the
	 * jump slots, for now, we rely on the symbol and hope no one
	 * is hijacking the jump table */
	exec_syms = ElfDebug::getLinkageSyms(mem, binpath);
	if (exec_syms) {
		dyn_syms->addSyms(exec_syms);
		delete exec_syms;
	}

	return dyn_syms;
}
