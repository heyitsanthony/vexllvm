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

#include <sys/ptrace.h>

#if defined(__amd64__)
#include <asm/ptrace-abi.h>
#include <sys/prctl.h>
#include "cpu/amd64cpustate.h"
#include "cpu/ptimgamd64.h"
#include "cpu/ptimgi386.h"
#endif


#include "guestcpustate.h"
#include "guestptimg.h"
#include "guestsnapshot.h"
#include "procargs.h"

#define DEFAULT_SC_NUM	100
#define DEFAULT_SC_GAP	1

void dumpIRSBs(void)
{
	fprintf(stderr, "shouldn't be decoding\n");
	exit(1);
}

class GuestChkPt : public GuestPTImg
{
public:
	virtual ~GuestChkPt(void) {}
	void doFixupSyscallRegs(int pid) { fixupSyscallRegs(pid); }
	void loadMemDiff(int pid, std::set<guest_ptr>& changed_maps);
	virtual void checkpoint(int pid, unsigned seq) = 0;
protected:
	GuestChkPt(const char* binpath, bool use_entry)
	: GuestPTImg(binpath, use_entry) {}
	virtual pid_t createSlurpedAttach(int pid);
 	virtual void handleChild(pid_t pid) {}
};

class GuestChkPtFast : public GuestChkPt
{
public:
	GuestChkPtFast(const char* binpath, bool use_entry)
	: GuestChkPt(binpath, use_entry) {}
	virtual ~GuestChkPtFast(void) {}
	void checkpoint(int pid, unsigned seq);
};

class GuestChkPtSlow : public GuestChkPt
{
public:
	GuestChkPtSlow(const char* binpath, bool use_entry)
	: GuestChkPt(binpath, use_entry) {}
	virtual ~GuestChkPtSlow(void) {}
	void checkpoint(int pid, unsigned seq);
};

pid_t GuestChkPt::createSlurpedAttach(int pid)
{
	int	err, status;

	// assert (entry_pt.o == 0 && "Only support attaching immediately");
	fprintf(stderr, "Attaching to PID=%d\n", pid);

	pt_arch = NEW_ARCH_PT;

	err = ptrace(PTRACE_ATTACH, pid, 0, NULL, NULL);
	assert (err != -1 && "Couldn't attach to process");

	wait(&status);
	fprintf(stderr, "ptrace status=%x\n", status);
	assert (WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP);
	fprintf(stderr, "Attached to PID=%d\n", pid);

	/* for multi-threaded apps */
	ptrace(PTRACE_SETOPTIONS, pid, NULL, PTRACE_O_TRACECLONE);

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
	}

	return GuestPTImg::createAttached<GuestChkPtFast>(
		pid,
		pa->getArgc(),
		pa->getArgv(),
		pa->getEnv());
}

double get_tv_diff(struct timeval* tv_begin, struct timeval* tv_end)
{
	return	(tv_end->tv_usec+(tv_end->tv_sec*1.0e6)) -
		(tv_begin->tv_usec+(tv_begin->tv_sec*1.0e6));
}

#define is_soft_dirty(x)	(((x) & (1ULL << 55)) != 0)
#define is_present(x)		(((x) & ((1ULL << 62) | (1UL << 63))) != 0)

static void stepSyscalls(int pid, unsigned skip_c = 1)
{
	for (unsigned i = 0; i < skip_c; i++) {
		int	err, status;

		/* complete last syscall */
		err = ptrace(PTRACE_SYSCALL, pid, 0, NULL, NULL);
		assert (err != -1);
		wait(&status);
		assert (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP);

		/* run to beginning of next syscall */
		err = ptrace(PTRACE_SYSCALL, pid, 0, NULL, NULL);
		assert (err != -1);
		wait(&status);
		assert (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP);
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
	foreach (it, changed_maps.begin(), changed_maps.end()) {
		ProcMap	*pm(NULL);

		foreach (it2, mappings.begin(), mappings.end()) {
			pm = *it2;
			if (pm->getBase() == it->offset) break;
			pm = NULL;
		}

		if (pm == NULL) {
			std::cerr << "COULD NOT FIND PM FOR MAPPING!\n";
			continue;
		}

		mappings.remove(pm);
		delete pm;
	}

	std::cerr << "Changed mappings: " << changed_maps.size() << '\n';
	changed_maps.clear();

	ProcMap::slurpMappings(pid, getMem(), mappings);
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

int main(int argc, char* argv[], char* envp[])
{
	GuestChkPt	*gs;
	int		pid;
	unsigned	sc_c;
	struct timeval	tv[2];

	if (argc != 2) {
		fprintf(stderr, "Usage: %s pid\n", argv[0]);
		return -1;
	}

	/* TODO: check kernel version for soft-dirty bit support? */

	pid = atoi(argv[1]);

	gettimeofday(&tv[0],NULL);
	gs = createAttached(pid);
	assert (gs != NULL && "could not create attached");

	/* take first snapshot */
	gs->save("chkpt-0000");
	gettimeofday(&tv[1],NULL);

	fprintf(stderr, "base_time: %g sec\n", get_tv_diff(&tv[0], &tv[1])/1e6);

	/* waiting on completion of the first snapshotted system call
	 * reset soft-dirty ptes, complete syscall, run to next, sshot, repeat
	 */
	sc_c = (getenv("VEXLLVM_SC_COUNT"))
		? atoi(getenv("VEXLLVM_SC_COUNT"))
		: DEFAULT_SC_NUM;
	for (unsigned i = 1; i < sc_c; i++)
		gs->checkpoint(pid, i);

	/* release the process */
	ptrace(PTRACE_DETACH, pid, 0, NULL, NULL);

	delete gs;

	return 0;
}
