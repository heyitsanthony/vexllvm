#include "Sugar.h"

#include <errno.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/ptrace.h>

#if defined(__amd64__)
#include <asm/ptrace-abi.h>
#include <sys/prctl.h>
#include "cpu/amd64cpustate.h"
#include "cpu/ptimgamd64.h"
#include "cpu/ptimgi386.h"
#endif


#include "guestmemdual.h"
#include "guestptmem.h"
#include "vexcpustate.h"
#include "guestptimg.h"
#include "guestsnapshot.h"
#include "procargs.h"

#include "guestabi.h"
#define DEFAULT_SC_NUM	100
#define DEFAULT_SC_GAP	1

static void clearSoftDirty(int pid);
static void stepHalfSyscall(int pid);
static void stepSyscalls(int pid, unsigned skip_c = 1);
static double get_tv_diff(struct timeval* tv_begin, struct timeval* tv_end);
static void patchVDSOs(class GuestChkPt* gs, int pid);

void dumpIRSBs(void)
{
	fprintf(stderr, "shouldn't be decoding\n");
	exit(1);
}

class GuestChkPt : public GuestPTImg
{
public:
	virtual ~GuestChkPt(void) {}
	void doFixupSyscallRegs(int pid) { fixupRegsPreSyscall(pid); }
	void loadMemDiff(int pid, std::set<guest_ptr>& changed_maps);
	virtual void checkpoint(int pid, unsigned seq) = 0;
	void splitStack(void);
	virtual void saveInitialChkPt(int pid);

	void waitForOpen(int pid, const char* path);

	pid_t getChildPID(void) const { return child_pid; }
protected:
	GuestChkPt(const char* binpath, bool use_entry=true)
	: GuestPTImg(binpath, use_entry) {}

	virtual pid_t createSlurpedAttach(int pid);
	virtual void handleChild(pid_t pid) { child_pid = pid; }
	pid_t	child_pid;
};

class GuestChkPtFast : public GuestChkPt
{
public:
	GuestChkPtFast(const char* binpath, bool use_entry=true)
	: GuestChkPt(binpath, use_entry) {}
	virtual ~GuestChkPtFast(void) {}
	void checkpoint(int pid, unsigned seq);
};

class GuestChkPtPrePost : public GuestChkPtFast
{
public:
	GuestChkPtPrePost(const char* binpath, bool use_entry=true)
	: GuestChkPtFast(binpath, use_entry) {}
	virtual ~GuestChkPtPrePost(void) {}
	void checkpoint(int pid, unsigned seq);
	virtual void saveInitialChkPt(int pid);
};

class GuestChkPtSlow : public GuestChkPt
{
public:
	GuestChkPtSlow(const char* binpath, bool use_entry=true)
	: GuestChkPt(binpath, use_entry) {}
	virtual ~GuestChkPtSlow(void) {}
	void checkpoint(int pid, unsigned seq);
};


void GuestChkPtPrePost::saveInitialChkPt(int pid)
{
	std::set<guest_ptr>	changed_maps;
	int			err;

	save("chkpt-0000-pre");
	err = symlink("chkpt-0000-pre", "chkpt-0000");
	assert (err == 0 && "Error saving chkpt-0000 symlink");

	/* complete initial system call */
	clearSoftDirty(pid);
	stepHalfSyscall(pid);

	mkdir("chkpt-0000-post", 0755);
	slurpRegisters(pid);
	loadMemDiff(pid, changed_maps);
	GuestSnapshot::saveDiff(
		this,
		"chkpt-0000-post",
		"chkpt-0000-pre",
		changed_maps);
}



void GuestChkPt::saveInitialChkPt(int pid) { save("chkpt-0000"); }

void GuestChkPt::splitStack(void)
{
	GuestMem::Mapping	m;

	if (mem->lookupMapping("[stack]", m) == false)
		return;

	/* change protection on every page individually
	 * because guestmem is stupid, it won't merge pages--
	 * the large segment will split on each mprotect */
	for (unsigned i = 0; i < m.length / (4096*4); i++) {
		guest_ptr	base(m.offset.o + i*(4096*4));
		mem->mprotect(base, 4*4096, PROT_READ);
		mem->mprotect(base, 4*4096, m.req_prot);
	}
}

pid_t GuestChkPt::createSlurpedAttach(int pid)
{
	int	err, status;

	// assert (entry_pt.o == 0 && "Only support attaching immediately");
	fprintf(stderr, "Attaching to PID=%d...\n", pid);

	pt_arch = NEW_ARCH_PT;

	err = ptrace(PTRACE_ATTACH, pid, 0, NULL, NULL);
	assert (err != -1 && "Couldn't attach to process");

	wait(&status);
	fprintf(stderr, "ptrace status=%x\n", status);
	assert (WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP);
	fprintf(stderr, "Attached to PID=%d !\n", pid);

	/* for multi-threaded apps */
	// ptrace(PTRACE_SETOPTIONS, pid, NULL, PTRACE_O_TRACECLONE);

	attachSyscall(pid);
	entry_pt = getCPUState()->getPC();

	return pid;
}

GuestChkPt* createAttached(int pid)
{
	ProcArgs	*pa;

	pa = new ProcArgs(pid);
	if (getenv("VEXLLVM_CHKPT_SLOW") != NULL) {
		return GuestPTImg::createAttached<GuestChkPtSlow>(
			pid,
			pa->getArgc(),
			pa->getArgv(),
			pa->getEnv());
	} else if (getenv("VEXLLVM_CHKPT_PREPOST") != NULL) {
		return GuestPTImg::createAttached<GuestChkPtPrePost>(
			pid,
			pa->getArgc(),
			pa->getArgv(),
			pa->getEnv());
	}

	return GuestPTImg::createAttached<GuestChkPtFast>(
		pid,
		pa->getArgc(),
		pa->getArgv(),
		pa->getEnv());
}

GuestChkPt* createNewProc(int argc, char** argv, char** envp)
{
	if (getenv("VEXLLVM_CHKPT_SLOW") != NULL) {
		return GuestPTImg::create<GuestChkPtSlow>(argc, argv, envp);
	} else if (getenv("VEXLLVM_CHKPT_PREPOST") != NULL) {
		return GuestPTImg::create<GuestChkPtPrePost>(argc, argv, envp);
	}

	return GuestPTImg::create<GuestChkPtFast>(argc, argv, envp);
}

static double get_tv_diff(struct timeval* tv_begin, struct timeval* tv_end)
{
	return	(tv_end->tv_usec+(tv_end->tv_sec*1.0e6)) -
		(tv_begin->tv_usec+(tv_begin->tv_sec*1.0e6));
}

#define is_soft_dirty(x)	(((x) & (1ULL << 55)) != 0)
#define is_present(x)		(((x) & ((1ULL << 62) | (1UL << 63))) != 0)

static void stepHalfSyscall(int pid)
{
	int	err, status;

	err = ptrace(PTRACE_SYSCALL, pid, 0, NULL, NULL);
	assert (err != -1);
	wait(&status);

	if (WIFEXITED(status)) {
		fprintf(stderr, "[ss_chkpt] PID=%d exited. Bye\n", pid);
		exit(0);
	}

	if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGSEGV) {
		fprintf(stderr,
			"[ss_chkpt] PID=%d exited with SIGSEGV. Bye\n", pid);
		exit(0);
	}

	if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGTRAP) {
		fprintf(stderr,
			"[ss_chkpt] Unexpected ptrace wait() status %x\n",
			status);
	}
	assert (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP);
}

static void stepSyscalls(int pid, unsigned skip_c)
{
	for (unsigned i = 0; i < skip_c; i++) {
		/* complete last syscall */
		stepHalfSyscall(pid);
		/* run to beginning of next syscall */
		stepHalfSyscall(pid);
	}
}

void GuestChkPt::loadMemDiff(int pid, std::set<guest_ptr>& changed_set)
{
	char				fname[128];
	int				fd;
	std::list<GuestMem::Mapping>	unchanged_maps, changed_maps;
	std::list<GuestMem::Mapping>	all_maps(getMem()->getMaps());

	sprintf(fname, "/proc/%d/pagemap", pid);
	fd = open(fname, O_RDONLY);
	assert (fd != -1);

	/* find all changes by looking for set soft dirty bits */
	foreach (it, all_maps.begin(), all_maps.end()) {
		guest_ptr	base(it->offset);
		unsigned	pg_c(it->length / 4096), i;
		uint64_t	*buf;
		unsigned	br;

		if (it->type == GuestMem::Mapping::VSYSPAGE) {
			unchanged_maps.push_back(*it);
			continue;
		}

		buf = new uint64_t[pg_c];
		lseek(fd, 8*(base.o / 4096), SEEK_SET);
		br = read(fd, buf, pg_c*8);
		assert (br == pg_c*8);

		for (i = 0; i < pg_c; i++) {
			if (is_soft_dirty(buf[i]) || !is_present(buf[i]))
				break;
		}

		if (i == pg_c) {
			unchanged_maps.push_back(*it);
		} else {
			changed_set.insert(it->offset);
			changed_maps.push_back(*it);
		}

		delete [] buf;
	}

	close(fd);

	/* remove mappings that have been changed so that the next
	 * slurp picks them up */
	for (auto &m : changed_maps) {
		bool erased = false;
		foreach (it, mappings.begin(), mappings.end()) {
			if ((*it)->getBase() == m.offset) {
				mappings.erase(it);
				erased = true;
				break;
			}
		}
		if (!erased) {
			std::cerr << "COULD NOT FIND PM FOR MAPPING!\n";
		}
	}

	std::cerr << "Changed mappings: " << changed_maps.size() << '\n';
	changed_maps.clear();

	ProcMap::slurpMappings(pid, getMem(), mappings);

	entry_pt = getCPUState()->getPC();
}

static void clearSoftDirty(int pid)
{
	char	fname[128];
	int	fd, bw;

	sprintf(fname, "/proc/%d/clear_refs", pid);
	fd = open(fname, O_WRONLY);
	assert (fd != -1);
	errno = 0;
	bw = write(fd, "4\n", 2);
	if (bw != 2) perror("couldn't clear refs");
	assert (bw == 2);
	close(fd);
}

void GuestChkPtPrePost::checkpoint(int pid, unsigned seq)
{
	char			fname[64], fname_last[64];
	struct timeval		tv[2];
	std::set<guest_ptr>	changed_maps;

	assert (seq > 0 && "fast checkpoint can't make base checkpoint");

	gettimeofday(&tv[0], NULL);

	/* run up to system call entry */
	clearSoftDirty(pid);

	/* run up to entry of syscall */
	stepHalfSyscall(pid);

	sprintf(fname, "chkpt-%04d-pre", seq);
	mkdir(fname, 0755);
	sprintf(fname_last, "chkpt-%04d-post", seq-1);

	slurpRegisters(pid);
	doFixupSyscallRegs(pid);
	loadMemDiff(pid, changed_maps);
	GuestSnapshot::saveDiff(this, fname, fname_last, changed_maps);

	changed_maps.clear();
	/* chkpt-nnnn-pre now saved */

	/* complete system call */
	clearSoftDirty(pid);

	/* last chkpt is chkpt-<seq>-pre, so use that as backing */
	strcpy(fname_last, fname);
	stepHalfSyscall(pid);
	sprintf(fname, "chkpt-%04d-post", seq);
	mkdir(fname, 0755);

	slurpRegisters(pid);
	/* post-syscall so no need to call doFixupSyscallRegs */
	loadMemDiff(pid, changed_maps);
	GuestSnapshot::saveDiff(this, fname, fname_last, changed_maps);
	/* chkpt-nnnn-post now saved */

	gettimeofday(&tv[1], NULL);
	fprintf(stderr, "chkpt_time %d: %g sec\n",
		seq,
		get_tv_diff(&tv[0], &tv[1])/1e6);

}

/* check pagemap for changes, only load that into guest */
/* create diff against last snapshot */
void GuestChkPtFast::checkpoint(int pid, unsigned seq)
{
	struct timeval		tv[2];
	char			fname[64], fname_last[64];
	std::set<guest_ptr>	changed_maps;

	assert (seq > 0 && "fast checkpoint can't make base checkpoint");

	/* reset dirty bits */
	clearSoftDirty(pid);

	/* run up to the beginning of next syscall */
	stepSyscalls(pid, DEFAULT_SC_GAP);

	sprintf(fname, "chkpt-%04d", seq);
	mkdir(fname, 0755);

	sprintf(fname_last, "chkpt-%04d", seq-1);
	gettimeofday(&tv[0], NULL);

	/* load it */
	slurpRegisters(pid);
	doFixupSyscallRegs(pid);

	loadMemDiff(pid, changed_maps);
	GuestSnapshot::saveDiff(this, fname, fname_last, changed_maps);

	gettimeofday(&tv[1], NULL);
	fprintf(stderr, "chkpt_time %d: %g sec\n",
		seq,
		get_tv_diff(&tv[0], &tv[1])/1e6);
}

void GuestChkPtSlow::checkpoint(int pid, unsigned seq)
{
	struct timeval		tv[2];
	char			fname[64], fname_last[64];
	std::set<guest_ptr>	changed_maps;

	assert (seq > 0 && "fast checkpoint can't make base checkpoint");

	stepSyscalls(pid, DEFAULT_SC_GAP);
	sprintf(fname, "chkpt-%04d", seq);
	mkdir(fname, 0755);

	sprintf(fname_last, "chkpt-%04d", seq-1);

	gettimeofday(&tv[0], NULL);

	/* load it */
	slurpRegisters(pid);
	doFixupSyscallRegs(pid);

	/* clear out old memory */
	mappings.clear();
	ProcMap::slurpMappings(pid, getMem(), mappings);
	GuestSnapshot::save(this, fname);

	gettimeofday(&tv[1], NULL);
	fprintf(stderr, "chkpt_time %d: %g sec\n",
		seq,
		get_tv_diff(&tv[0], &tv[1])/1e6);
}

static void patchVDSOs(GuestChkPt* gs, int pid)
{
	GuestMem	*jit_mem, *pt_mem, *dual_mem;
	bool		ok;
	
	jit_mem = gs->getMem();
	pt_mem = new GuestPTMem(gs, pid);
	dual_mem = GuestMemDual::createImported(jit_mem, pt_mem);
	
	gs->setMem(dual_mem);
	ok = gs->patchVDSO();
	assert (ok && "Did not patch VDSO?");
	gs->setMem(jit_mem);

	delete dual_mem;
	delete pt_mem;
}

/* is it worth tracking fds etc for more sophisticated syscall latching? */
#include <sys/syscall.h>
void GuestChkPt::waitForOpen(int pid, const char* path)
{
	assert (getArch() == Arch::X86_64);

	clearSoftDirty(pid);
	while (1) {
		SyscallParams		sp(getSyscallParams());
		if (sp.getSyscall() == SYS_open) {
			std::set<guest_ptr>	changed_maps;
			std::string		s;
			
			loadMemDiff(pid, changed_maps);
			s = getMem()->readString(guest_ptr(sp.getArg(0)));

			/* good to go? */
			if (s == path) break;

			clearSoftDirty(pid);
		}
		stepSyscalls(pid);
		slurpRegisters(pid);
	}
}

static int is_num(const char* s)
{ for (; *s; s++) { if (*s < '0' || *s > '9') return 0; } return 1; }

int main(int argc, char* argv[], char* envp[])
{
	GuestChkPt	*gs;
	int		pid;
	unsigned	sc_c;
	struct timeval	tv[2];

	if (argc != 2) {
		fprintf(stderr, "Usage: %s [pid | argv]\n", argv[0]);
		return -1;
	}

	/* TODO: check kernel version for soft-dirty bit support? */

	VexCPUState::registerCPUs();

	gettimeofday(&tv[0],NULL);

	if (is_num(argv[1])) {
		pid = atoi(argv[1]);
		gs = createAttached(pid);
		assert (gs != NULL && "could not create attached");
	} else {
		gs = createNewProc(argc-1, argv+1, envp);
		assert (gs != NULL && "could not create new proc");
		pid = gs->getChildPID();
	}

	/* break up stack so checkpointing is cheaper */
	/* XXX: doesn't work because of procmap slurping. crap! */
	// gs->splitStack();
	
	
	/* patch over vdso page so we can get timer calls */
	fprintf(stderr, "[ss_chkpt] Patching VDSO\n");

	/* patching vdso screws up the syscall state;
	 * so reset to before syscall, then half step to pre-syscall */
	patchVDSOs(gs, pid);
	/* gs registers are fixed up to immediately before syscall */
	gs->getPTArch()->pushRegisters();
	stepHalfSyscall(pid);
	/* now at pre-syscall */
	
	if (getenv("VEXLLVM_CHKPT_WAITOPEN") != NULL)
		gs->waitForOpen(pid, getenv("VEXLLVM_CHKPT_WAITOPEN"));


	/* take first snapshot */
	gs->saveInitialChkPt(pid);

	gettimeofday(&tv[1],NULL);

	fprintf(stderr, "base_time: %g sec\n", get_tv_diff(&tv[0], &tv[1])/1e6);

	/* waiting on completion of the first snapshotted system call
	 * reset soft-dirty ptes, complete syscall, run to next, sshot, repeat
	 */
	sc_c = (getenv("VEXLLVM_SC_COUNT"))
		? atoi(getenv("VEXLLVM_SC_COUNT"))
		: DEFAULT_SC_NUM;
	for (unsigned i = 1; i < sc_c; i++) {
		gs->checkpoint(pid, i);
	}

	/* release the process */
	ptrace(PTRACE_DETACH, pid, 0, NULL, NULL);

	delete gs;

	return 0;
}
