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
#include "guestabi.h"
#include "ptcpustate.h"

#if defined(__amd64__)
#include <asm/ptrace-abi.h>
#include <sys/prctl.h>
#include "cpu/ptimgamd64.h"
#include "cpu/ptimgi386.h"
#endif

#ifdef __arm__
#include <linux/prctl.h>
#include <sys/prctl.h>
#include "cpu/ptimgarm.h"
#endif

#define ANDROID_SYS_BEGIN	0x40000000

#define IS_SIGTRAP(x) (WIFSTOPPED(x) && WSTOPSIG(x) == SIGTRAP)

#include "guestptmem.h"

GuestPTImg::GuestPTImg(const char* binpath, bool use_entry)
: Guest(binpath)
, pt_arch(NULL)
, pt_shadow(NULL)
, arch(Arch::getHostArch())
, symbols(NULL)
, dyn_symbols(NULL)
{
	bool	use_32bit_arch;

	mem = new GuestMem();
	ProcMap::dump_maps = (getenv("VEXLLVM_DUMP_MAPS")) ? true : false;

	use_32bit_arch = (getenv("VEXLLVM_32_ARCH") != NULL);

	entry_pt = guest_ptr(0);
	if (binpath != NULL && use_entry)  {
		ElfImg	*img = ElfImg::create(binpath, false, false);
		assert (img != NULL && "DOES BINARY EXIST?");
		assert(img->getArch() == getArch() || use_32bit_arch);

		entry_pt = img->getEntryPoint();

		/* XXXX: gross hack for android */
		if (arch == Arch::ARM && use_entry && ElfImg::isDynamic(binpath))
			entry_pt.o |= ANDROID_SYS_BEGIN;

		delete img;
	}

	if (use_32bit_arch) {
		assert (arch == Arch::X86_64);
		arch = Arch::I386;
		mem->mark32Bit();
	}

	cpu_state = GuestCPUState::create(getArch());
	abi = GuestABI::create(*this);
}

GuestPTImg::~GuestPTImg(void)
{
	delete pt_arch;
	// delete pt_shadow; aliased if available
	delete symbols;
	delete dyn_symbols;
}

void GuestPTImg::handleChild(pid_t pid)
{
	ptrace(PTRACE_KILL, pid, NULL, NULL);
	wait(NULL);
}

void GuestPTImg::attachSyscall(int pid)
{
	int	err, status;

	/* NOTE: this stops immediately before a syscall... */
	err = ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
	assert (err != -1);
	wait(&status);
	assert (IS_SIGTRAP(status));
//	fprintf(stderr, "Got syscall from PID=%d\n", pid);
	slurpBrains(pid);
	fixupRegsPreSyscall(pid);
}

/* fixes the registers when on a PTRACE_SYSCALL at syscall entry */
void GuestPTImg::fixupRegsPreSyscall(int pid)
{
	int	old_pid = pt_arch->getPID();
	if (pid) pt_arch->setPID(pid);
	pt_arch->fixupRegsPreSyscall();
	pt_arch->setPID(old_pid);
}

pid_t GuestPTImg::createSlurpedAttach(int pid)
{
	int	err, status;

	// assert (entry_pt.o == 0 && "Only support attaching immediately");
	fprintf(stderr, "Attaching to PID=%d\n", pid);

	SETUP_ARCH_PT

	err = ptrace(PTRACE_ATTACH, pid, NULL, NULL);
	assert (err != -1 && "Couldn't attach to process");

	wait(&status);
	fprintf(stderr, "ptrace status=%x\n", status);
	assert (WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP);
	fprintf(stderr, "Attached to PID=%d\n", pid);

	if (getenv("VEXLLVM_NOSYSCALL") == NULL)
		attachSyscall(pid);
	else
		slurpBrains(pid);

	entry_pt = getCPUState()->getPC();

	/* release the process */
	ptrace(PTRACE_DETACH, pid, NULL, NULL);

	return pid;
}

static void setupTraceMe(void)
{
	/* don't keep running if parent dies */
	prctl(PR_SET_PDEATHSIG, SIGKILL);
	prctl(PR_SET_PDEATHSIG, SIGSEGV);
	prctl(PR_SET_PDEATHSIG, SIGILL);
	prctl(PR_SET_PDEATHSIG, SIGBUS);
	prctl(PR_SET_PDEATHSIG, SIGABRT);
	ptrace(PTRACE_TRACEME, 0, NULL, NULL);
}

static void setupChild(char *const argv[], char *const envp[])
{
	int     err, new_env = 0;

	setupTraceMe();

	if (getenv("VEXLLVM_LIBRARY_PATH")) {
		setenv("LD_LIBRARY_PATH", getenv("VEXLLVM_LIBRARY_PATH"), 1);
		new_env = 1;
	}

	if (getenv("VEXLLVM_PRELOAD")) {
		setenv("LD_PRELOAD", getenv("VEXLLVM_PRELOAD"), 1);
		new_env = 1;
	}

	if (new_env) {
		err = execvp(argv[0], argv);
	} else {
		err = execvpe(argv[0], argv, envp);
	}

	assert (err != -1 && "EXECVE FAILED. NO PTIMG!");
}


pid_t GuestPTImg::createFromGuest(Guest* gs)
{
	pid_t		pid;
	int		status;
	guest_ptr	cur_pc, break_addr;

	fprintf(stderr, "[GuestPTImg] creating ptimg from guest\n");

	pid = fork();
	if (pid < 0) return pid;
	if (pid == 0) {
		/* child */
		setupTraceMe();

		/* wait for parent by SIGTRAPing self */
		kill(getpid(), SIGTRAP);

		/* parent should have set breakpoint at current IP
		 * ... jump to it */
		cpu_state = gs->getCPUState();
		SETUP_ARCH_PT

		pt_arch->restore();

		assert (0 == 1 && "OOPS");
		exit(-1);
	}

	SETUP_ARCH_PT

	/* wait for child to SIGTRAP itself */
	waitpid(pid, &status, 0);
	assert (IS_SIGTRAP(status));

	/* Trapped the process on execve-- binary is loaded, but not linked */
	/* overwrite entry with BP. */
	cur_pc = gs->getCPUState()->getPC();
	fprintf(stderr,
		"[GuestPTImg] setting bp on cur_pc=%p\n", (void*)cur_pc.o);
	setBreakpoint(cur_pc);

	/* run until child hits cur_pc */
	waitForEntry(pid);

	break_addr = undoBreakpoint();
	assert (break_addr == cur_pc && "Did not break at entry");

	/* cleanup bp */
	resetBreakpoint(cur_pc);

	/* copy guest state into our state, destroy old guest */
	cpu_state = gs->cpu_state;
	abi = GuestABI::create(*this);
	mem = gs->mem;
	bin_path = gs->bin_path;
	argv_ptrs = gs->getArgvPtrs();
#ifndef BROKEN_OSDI
	argc_ptr = gs->getArgcPtr();
#endif

	symbols = new Symbols(*gs->getSymbols());

	/* this needs guestptimg to be a 'friend' of guest */
	gs->cpu_state = NULL;
	gs->mem = NULL;
	gs->bin_path = NULL;
	delete gs;

	entry_pt = cur_pc;

	fprintf(stderr, "[GuestPTImg] Guest state cloned to process %d\n", pid);
	return pid;
}

/* this is mainly to support programs that are shared libraries;
 * use VEXLLVM_WAIT_SYSNR=sys_munmap */
pid_t GuestPTImg::createSlurpedOnSyscall(
	int argc, char *const argv[], char *const envp[],
	unsigned sys_nr)
{
	int			status;
	pid_t			pid;
	guest_ptr		break_addr;
	unsigned		sc_c = 0;

	pid = fork();
	if (pid == 0) setupChild(argv, envp);
	if (pid < 0) return pid;

	SETUP_ARCH_PT

	/* wait for child to call execve and send us a trap signal */
	wait(&status);
	assert (IS_SIGTRAP(status));

	/* run up to expected syscall */
	while (1) {
		/* entry into syscall */
		ptrace(PTRACE_SYSCALL, pid, 0, NULL, NULL);
		wait(&status);
		if (!IS_SIGTRAP(status)) {
			std::cerr << "[GuestPTImg] Child exited before sys_nr="
				<< sys_nr << '\n';
			return 0;
		}

		slurpRegisters(pid);
		SyscallParams	sp(getSyscallParams());
		if (sp.getSyscall() == sys_nr) {
			std::cerr << "[GuestPTImg] got sys_nr=" << sys_nr
				<< " on syscall #" << sc_c << '\n';
			break;
		}

		/* step through syscall */
		ptrace(PTRACE_SYSCALL, pid, 0, NULL, NULL);
		wait(&status);
		if (!IS_SIGTRAP(status)) {
			std::cerr << "[GuestPTImg] Child exited before sys_nr="
				<< sys_nr << '\n';
			return 0;
		}
		sc_c++;
	}

	if (ProcMap::dump_maps) dumpSelfMap();

	std::cerr << "[GuestPTImg] Loading child process\n";
	/* slurp brains after trap code is removed so that we don't
	 * copy the trap code into the parent process */
	slurpBrains(pid);

	// can't slurp argptrs because not on main(). hm.
	// slurpArgPtrs(pid, argv);

	entry_pt = getCPUState()->getPC();
	return pid;
}

pid_t GuestPTImg::createSlurpedChild(
	int argc, char *const argv[], char *const envp[])
{
	int			status;
	pid_t			pid;
	guest_ptr		break_addr;

	assert (entry_pt.o && "No entry point given to slurp");

	pid = fork();
	if (pid == 0)
		setupChild(argv, envp);

	/* failed to create child */
	if (pid < 0) return pid;

	SETUP_ARCH_PT

	/* wait for child to call execve and send us a trap signal */
	wait(&status);
	assert (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP);

	/* Trapped the process on execve-- binary is loaded, but not linked */
	/* overwrite entry with BP. */
	setBreakpoint(entry_pt);

	/* run until child hits entry point */
	waitForEntry(pid);

	break_addr = undoBreakpoint();
	assert (break_addr == entry_pt && "Did not break at entry");

	/* hit the entry point, everything should be linked now-- load it
	 * into our context! */
	/* cleanup bp */
	resetBreakpoint(entry_pt);

	if (ProcMap::dump_maps) dumpSelfMap();

	std::cerr << "[GuestPTImg] Loading child process\n";
	/* slurp brains after trap code is removed so that we don't
	 * copy the trap code into the parent process */
	slurpBrains(pid);
	slurpArgPtrs(argv);

	return pid;
}

void GuestPTImg::slurpArgPtrs(char *const argv[])
{
	int	argc = 0;

#if (defined(__amd64__) || defined(__arm__) || defined(__x86__))
	/* XXX there should be a better place for this */
	/* from glibc, sysdeps/x86_64/elf/start.S */

	/* from glibc-ports
	 * 0(sp)	argc
	 * 4(sp)	argv[0]
	 * ...
	 * (4*argc)(sp)	NULL
	 */
	guest_ptr		in_argv;

	argv_ptrs.clear();
	argc_ptr = pt_arch->getPTCPU().getStackPtr();
	argc = mem->readNative(argc_ptr);
	in_argv = guest_ptr(mem->readNative(pt_arch->getPTCPU().getStackPtr(), 1));
#ifdef __arm__
	/* so so stupid, but I can't figure out how to get argv[0]! */
	if (!in_argv) {
		in_argv = guest_ptr(
			mem->readNative(pt_arch->getPTCPU().getStackPtr(), 2));
	}

	while (strcmp((const char*)mem->getHostPtr(in_argv), argv[0]) != 0)
		in_argv.o--;
#endif

	for (int i = 0; i < argc; i++) {
		int	arg_len;
		char	argbuf[1024];

		argv_ptrs.push_back(in_argv);

		arg_len = strlen(argv[i]);
		assert (arg_len < 1024 && "arg_len larger than argbuf");

		mem->memcpy(argbuf, in_argv, arg_len+1);
		assert (strncmp(argbuf, argv[i], arg_len) == 0);
		in_argv.o += mem->strlen(in_argv) + 1;
	}
#else
	fprintf(stderr,
		"[VEXLLVM] WARNING: can not get argv for current arch\n");
#endif
}

void GuestPTImg::slurpRegisters(pid_t pid)
{
	pid_t	old_pid(pt_arch->getPID());
	if (pid != 0) pt_arch->setPID(pid);
	pt_arch->slurpRegisters();
	pt_arch->setPID(old_pid);
}

void GuestPTImg::slurpBrains(pid_t pid)
{
	ProcMap::slurpMappings(pid, mem, mappings);
	slurpRegisters(pid);
	slurpThreads();
}

void GuestPTImg::slurpThreads(void)
{
	std::list<GuestMem::Mapping>	m(mem->getMaps());
	std::vector<int>		t_pids;

	/* get thread pids from stack names */
	foreach (it, m.begin(), m.end()) {
		int	t_pid, c;
		c = sscanf(it->getName().c_str(), "[stack:%d]", &t_pid);
		if (c != 1) continue;
		t_pids.push_back(t_pid);
	}

	if (t_pids.empty()) return;

	/* load all thread registers */
	thread_cpus.resize(t_pids.size());
	for (unsigned i = 0; i < t_pids.size(); i++) {
		GuestCPUState	*cur_cpu(GuestCPUState::create(getArch()));
		int		err, status;

		/* slurpRegisters stores to cpu_state; temporarily swap out */
		thread_cpus[i] = cpu_state;
		cpu_state = cur_cpu;

		err = ptrace(PTRACE_ATTACH, t_pids[i], NULL, NULL);
		assert (err != -1);

		/* NB: the __WALL flag was took forever to figure out; ugh */
		status = 0;
		err = waitpid(t_pids[i], &status, __WALL);
		assert (err != -1);

		slurpRegisters(t_pids[i]);

		err = ptrace(PTRACE_DETACH, t_pids[i], NULL, NULL);
		assert (err != -1);

		cpu_state = thread_cpus[i];
		thread_cpus[i] = cur_cpu;
	}
}

void GuestPTImg::waitForEntry(int pid)
{
	int		err, status;
	const char	*fake_cpuid;

	fake_cpuid = getenv("VEXLLVM_FAKE_CPUID");
	if (fake_cpuid != NULL) {
		pt_arch->setFakeInfo(fake_cpuid);
		if (pt_arch->stepInitFixup() == false) {
			stackTrace(std::cerr, getBinaryPath(), pid);
			abort();
		}
		/* should end on breakpoint instruction */
		return;
	}

	err = ptrace(PTRACE_CONT, pid, NULL, NULL);
	assert (err != -1 && "bad ptrace_cont");
	waitpid(pid, &status, 0);

	assert (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP);
}

void GuestPTImg::setBreakpoint(guest_ptr addr)
{
	if (breakpoints.count(addr))
		return;
	assert (pt_arch);
	breakpoints[addr] = pt_arch->getPTCPU().setBreakpoint(addr);
}


guest_ptr GuestPTImg::undoBreakpoint()
{ return pt_arch->getPTCPU().undoBreakpoint(); }

void GuestPTImg::resetBreakpoint(guest_ptr addr)
{
	uint64_t	old_v;

	assert (breakpoints.count(addr) && "Resetting non-BP!");

	old_v = breakpoints[addr];
	pt_arch->getPTCPU().resetBreakpoint(addr, old_v);
	breakpoints.erase(addr);
}

const Symbols* GuestPTImg::getSymbols(void) const
{
	if (!symbols) symbols = loadSymbols(mappings);
	return symbols;
}

Symbols* GuestPTImg::getSymbols(void)
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

/* This is a really shitty hack to force VEXLLVM_PRELOAD libraries
 * to override default symbols. In the future, we need to support
 * duplicate symbols */
void GuestPTImg::forcePreloads(
	Symbols			*symbols,
	std::set<std::string>	&mmap_fnames,
	const ptr_list_t<ProcMap>& mappings)
{
	const char*	preload_str;
	unsigned	preload_len;
	char*		preload_libs;

	preload_str = getenv("VEXLLVM_PRELOAD");
	if (preload_str == NULL) return;

	preload_len = strlen(preload_str);
	preload_libs = strdup(preload_str);

	for (unsigned i = 0; i < preload_len; i++)
		if (preload_libs[i] == ':')
			preload_libs[i] = '\0';

	for (	char* cur_lib = preload_libs;
		(cur_lib - preload_libs) < preload_len;
		cur_lib += strlen(cur_lib) + 1)
	{
		guest_ptr	base(0);

		for (auto &m : mappings) {
			if (m->getLib() == cur_lib) {
				base = m->getBase();
				break;
			}
		}

		if (base.o == 0) {
			std::cerr << "[Warning] "
				<< "Could not find VEXLLVM_PRELOAD library '"
				<< cur_lib << "'\n";
			continue;
		}

		addLibrarySyms(cur_lib, base, symbols);
		mmap_fnames.insert(cur_lib);
	}

	free(preload_libs);
}

Symbols* GuestPTImg::loadSymbols(const ptr_list_t<ProcMap>& mappings)
{
	Symbols			*symbols;
	std::set<std::string>	mmap_fnames;

	symbols = new Symbols();

	forcePreloads(symbols, mmap_fnames, mappings);

	for (auto &mapping : mappings) {
		std::string	libname(mapping->getLib());
		guest_ptr	base(mapping->getBase());

		if (libname.size() == 0)
			continue;

		/* already seen it? */
		if (mmap_fnames.count(libname) != 0)
			continue;

		/* new fname, try to load the symbols */
		addLibrarySyms(libname.c_str(), base, symbols);

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
