#include <sys/mman.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <unistd.h>

#include "procmap.h"

bool ProcMap::dump_maps = false;

int ProcMap::getProt(void) const
{
	int	prot = 0;

	if (perms[0] == 'r') prot |= PROT_READ;
	if (perms[1] == 'w') prot |= PROT_WRITE;
	if (perms[2] == 'x') prot |= PROT_EXEC;

	return prot;
}

#ifdef __amd64__
#define STACK_EXTEND_BYTES	0xf0000

#include <setjmp.h>
#include <signal.h>

#define PROBE_BASE	((volatile char*)0xffffffffff5fe000)
#define PROBE_PAGES	2

static sigjmp_buf	probe_env;
static void probe_sig(int signum) { siglongjmp(probe_env, 1); }

static bool probe_fake_timers(void)
{
	struct sigaction	act, oldact;
	bool			found_fakes;

	memset(&act, 0, sizeof(act));
	act.sa_handler = probe_sig;
	sigaction(SIGSEGV, &act, &oldact);

	if (sigsetjmp(probe_env, 1) == 0) {
		/* probing */
		found_fakes = true;
		for (int i = 0; i < PROBE_PAGES; i++) {
			volatile char c = PROBE_BASE[4096*i];
			(void)c;
		}
	} else {
		/* faulted */
		found_fakes = false;
	}

	sigaction(SIGSEGV, &oldact, NULL);
	return found_fakes;
}

#else
#define STACK_EXTEND_BYTES	0x1000
#endif

#if defined(__arm__)
#define STACK_MAP_FLAGS	\
	(MAP_GROWSDOWN | MAP_PRIVATE | MAP_ANONYMOUS)
#else
#define STACK_MAP_FLAGS \
	(MAP_GROWSDOWN | MAP_STACK | MAP_PRIVATE | MAP_ANONYMOUS)
#endif


void ProcMap::mapStack(pid_t pid)
{
	int			prot;

	assert (mmap_fd == -1);

	prot = getProt();

	mem_begin.o -= STACK_EXTEND_BYTES;

	int res = mem->mmap(
		mmap_base,
		mem_begin,
		getByteCount(),
		prot,
		STACK_MAP_FLAGS | MAP_FIXED,
		-1,
		0);

	assert (!res && "Could not map stack region");

	copyRange(pid,
		mem_begin + STACK_EXTEND_BYTES,
		mem_end);
}

bool ProcMap::procMemCopy(pid_t pid, guest_ptr m_beg, guest_ptr m_end)
{
	char	path[128];
	off_t	off;
	ssize_t	br;
	int	fd;
	bool	ret = false;

	if (!copy) return true;

	sprintf(path, "/proc/%d/mem", pid);
	fd = open(path, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "Could not open %s\n", path);
		return false;
	}

	off = lseek(fd, m_beg.o, SEEK_SET);
	if (off != (off_t)m_beg.o) {
		fprintf(stderr, "Could not seek to %p\n", (void*)m_beg.o);
		goto done;
	}

	br = read(fd, mem->getHostPtr(m_beg), m_end - m_beg);
	if (br != (ssize_t)(m_end - m_beg)) {
		fprintf(stderr, "Could not read %d bytes. Got=%d. error=%s\n",
			(int)(m_end - m_beg),
			(int)br,
			strerror(errno));
		goto done;
	}

	ret = true;
done:
	close(fd);
	return ret;
}

void ProcMap::mapAnon(pid_t pid)
{
	int			prot, flags;

	assert (mmap_fd == -1);

	prot = getProt();
	flags = MAP_PRIVATE | MAP_ANONYMOUS;

	int res = mem->mmap(
		mmap_base,
		mem_begin,
		getByteCount(),
		prot,
		flags | MAP_FIXED,
		mmap_fd,
		off);
	if (res) {
		fprintf(stderr, "Failed to map base=%p. len=%p.\n",
			(void*)(long)mmap_base.o, (void*)(long)getByteCount());
	}
	assert (!res && "Could not map anonymous region");

	ptraceCopy(pid, prot);
}

void ProcMap::mapLib(pid_t pid)
{
	int			prot, flags;

	if (	strcmp(libname, "[vsyscall]") == 0 ||
		strcmp(libname, "[vectors]") == 0 ||
		strcmp(libname, "[FAKEtimers]") == 0)
	{
		/* the infamous syspage */
		char	*sysbuf = new char[getByteCount()];
		memcpy(sysbuf, (void*)(getBase().o), getByteCount());
		mem->addSysPage(getBase(), sysbuf, getByteCount());
		return;
	}

	if (strcmp(libname, "[stack]") == 0) {
		mapStack(pid);
		return;
	}

	/* don't load /dev/ files-- had a problem with alsa stuff in dosbox */
	if (strncmp(libname, "/dev/", 5) == 0) {
		fprintf(stderr, "[ProcMap] Ignoring device file %s\n", libname);
		libname[0] = 'X';
		libname[1] = '\0';
		mmap_fd = -1;
	} else {
		struct stat	s;
		mmap_fd = open(libname, O_RDONLY);
		assert (mmap_fd != -1 || stat(libname, &s) == -1);
	}

	if (mmap_fd == -1) {
		mapAnon(pid);
		return;
	}

	flags = MAP_PRIVATE;
	prot = getProt();

	assert (mmap_fd != -1);

	int res = mem->mmap(
		mmap_base,
		mem_begin,
		getByteCount(),
		prot,
		flags | MAP_FIXED,
		mmap_fd,
		off);
	assert (!res && "Could not map library region");

	assert (mmap_base == mem_begin && "Could not map to same address");
	if (flags != MAP_SHARED)
		ptraceCopy(pid, prot);
}

void ProcMap::copyRange(
	pid_t pid, guest_ptr m_beg, guest_ptr m_end)
{
	/* no need to copy reserved pages */
	if (getProt() == 0)
		return;

	if (procMemCopy(pid, m_beg, m_end))
		return;

	ptraceCopyRange(pid, m_beg, m_end);
}

void ProcMap::ptraceCopyRange(
	pid_t pid, guest_ptr m_beg, guest_ptr m_end)
{
	assert((m_beg & (sizeof(long) - 1)) == 0);
	assert((m_end & (sizeof(long) - 1)) == 0);

	guest_ptr copy_addr;

	if (!copy) return;

	copy_addr = m_beg;
	errno = 0;
	while (copy_addr != m_end) {
		long peek_data;
		peek_data = ptrace(PTRACE_PEEKDATA, pid, copy_addr.o, NULL);
		if (peek_data == -1 && errno) {
			fprintf(stderr,
				"Bad access: addr=%p err=%s\n",
				(void*)copy_addr.o, strerror(errno));
		}
		assert (peek_data != -1 || errno == 0);

		if (mem->read<long>(copy_addr) != peek_data) {
			mem->write(copy_addr, peek_data);
		}
		copy_addr.o += sizeof(long);
	}
}

void ProcMap::ptraceCopy(pid_t pid, int prot)
{
	if (!copy) return;

	if (!(prot & PROT_READ)) return;

	if (!(prot & PROT_WRITE)) {
		int res = mem->mprotect(mmap_base, getByteCount(),
			prot | PROT_WRITE);
		assert(!res && "granting temporary write permission failed");
	}

	copyRange(pid, mem_begin, mem_end);

	if (!(prot & PROT_WRITE)) {
		int res = mem->mprotect(mmap_base, getByteCount(), prot);
		assert(!res && "removing temporary write permission failed");
	}
}

ProcMap* ProcMap::create(
	GuestMem* in_mem, pid_t pid, const char* mapline, bool _copy)
{
	ProcMap	*pm(new ProcMap(in_mem, pid, mapline, _copy));

	if (pm->mmap_fd == -1) {
		delete pm;
		pm = NULL;
	}

	return pm;
}


ProcMap::ProcMap(GuestMem* in_mem, pid_t pid, const char* mapline, bool _copy)
: mmap_base(0)
, mmap_fd(-1)
, mem(in_mem)
, copy(_copy)
{
	int		rc;

	libname[0] = '\0';
	rc = sscanf(mapline, "%p-%p %s %x %d:%d %d %s",
		(void**)&mem_begin,
		(void**)&mem_end,
		perms,
		&off,
		&t[0],
		&t[1],
		&xxx,
		libname);

	assert (rc >= 0);

	/* don't remap */
	if (mem->isMapped(mem_begin))
		return;

	if (dump_maps) fprintf(stderr, "ProcMap: %s", mapline);

	/* now map it in */
	if (strlen(libname) <= 0) {
		mapAnon(pid);
		return;
	}

	mapLib(pid);
	if (strcmp(libname, "[stack]") == 0) {
		mem->setType(getBase(), GuestMem::Mapping::STACK);
	} else if (strcmp(libname, "[heap]") == 0) {
		mem->setType(getBase(), GuestMem::Mapping::HEAP);
	}
}

ProcMap::~ProcMap(void)
{
	if (mmap_fd) close(mmap_fd);
	if (mmap_base) mem->munmap(mmap_base, getByteCount());
}

void ProcMap::slurpMappings(
	pid_t pid,
	GuestMem* m,
	PtrList<ProcMap>& ents,
	bool do_copy)
{
	ProcMap	*mapping;
	FILE	*f;
	char	map_fname[256];

	sprintf(map_fname, "/proc/%d/maps", pid);
	f = fopen(map_fname, "r");
	assert (f != NULL && "Could not open /proc/.../maps");

	while (!feof(f)) {
		char		line_buf[256];

		if (fgets(line_buf, 256, f) == NULL)
			break;

		mapping = ProcMap::create(m, pid, line_buf, do_copy);
		if (mapping == NULL)
			continue;

		ents.add(mapping);
		m->nameMapping(mapping->getBase(), mapping->getLib());
	}
	fclose(f);

#ifdef __amd64__
	/* handle the hidden timers page */
	/* there is nothing I don't hate about this */
	if (probe_fake_timers()) {
		// fprintf(stderr, "[ProcMap] !!! Found fake timer pages\n");
		mapping = ProcMap::create(
			m,
			pid,
			"ffffffffff5fe000-ffffffffff600000  "
			"r--p 00000000 00:00 0 [FAKEtimers]");
		if (mapping != NULL) {
			ents.add(mapping);
			m->nameMapping(mapping->getBase(), mapping->getLib());
		}
	}
#endif
}
