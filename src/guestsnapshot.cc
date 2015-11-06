#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "Sugar.h"
#include "symbols.h"
#include "util.h"
#include "vexcpustate.h"
#include "guestsnapshot.h"
#include "guestabi.h"
#include "cpu/i386cpustate.h"
#include "cpu/i386windowsabi.h"
#include <algorithm>

using namespace std;

#if defined(__amd64__)
#define SYSPAGE_ADDR	guest_ptr(0xffffffffff600000)
#elif defined(__arm__)
#define SYSPAGE_ADDR	guest_ptr(0xffff0000)
#endif

#define BUFSZ		1024
#define BUFSZ_STR	"1024"	/* ugh, stringification is fucked */

static void saveMappings(const Guest* g, const char* dirpath);

GuestSnapshot* GuestSnapshot::create(const char* dirpath)
{
	GuestSnapshot	*ret;

	ret = new GuestSnapshot(dirpath);
	if (ret->is_valid == false) {
		delete ret;
		return NULL;
	}

	return ret;
}

#define SETUP_F_R_MAYBE(x)		\
	{ FILE		*f;		\
	char 		buf[BUFSZ];	\
	snprintf(buf, BUFSZ, "%s/%s", srcdir.c_str(), x);	\
	f = fopen(buf, "r");
#define SETUP_F_R(x)			\
	SETUP_F_R_MAYBE(x)		\
	assert (f != NULL && "failed to open "#x);
#define END_F()	fclose(f); }

GuestSnapshot::GuestSnapshot(const char* dirpath)
: Guest(NULL)
, is_valid(false)
, syms(NULL)
, dyn_syms(NULL)
, srcdir(dirpath)
{
	ssize_t	sz;

	SETUP_F_R("binpath")
	const char	*fget_buf;
	fget_buf = fgets(buf, BUFSZ, f);
	assert (fget_buf == buf);
	setBinPath(buf);
	END_F()

	SETUP_F_R("arch")
	sz = fread(&arch, sizeof(arch), 1, f);
	assert (sz == 1);
	END_F()

	SETUP_F_R("entry")
	sz = fread(&entry_pt, sizeof(entry_pt), 1, f);
	assert (sz == 1);
	END_F()

	loadThreads();

	SETUP_F_R("argv")
	argv_ptrs.clear();
	uint64_t	p;
	while (fscanf(f, "%p\n", ((void**)&p)) == 1) {
		argv_ptrs.push_back(guest_ptr(p));
	}
	END_F()

	argc_ptr.o = 0;
	SETUP_F_R_MAYBE("argc")
	if (f != NULL) {
		uint64_t	p;
		if (fscanf(f, "%p\n", ((void**)&p)) == 1)
			argc_ptr.o = p;
		END_F()
	}

	loadMappings();
	syms = loadSymbols("syms");
	dyn_syms = loadSymbols("dynsyms");


	/* XXX: super-lame windows detection */
	SETUP_F_R_MAYBE("platform/process_cookie");
	if (f == NULL) {
		abi = GuestABI::create(this);
	} else {
		abi = new I386WindowsABI(this);
		END_F()
	}

	is_valid = true;
}

void GuestSnapshot::loadMappings(void)
{
	SETUP_F_R("mapinfo")
	assert (mem == NULL);

	mem = new GuestMem();
	switch (arch) {
	case Arch::MIPS32:
	case Arch::ARM:
	case Arch::I386:
		mem->mark32Bit();
		break;
	case Arch::X86_64:
		break;
	default:
		assert (0 == 1 && "UNKNOWN ARCH");
		break;
	}

	while (fgets(buf, BUFSZ, f) != NULL) {
		guest_ptr			begin, end, mmap_addr;
		size_t				length;
		int				prot, fd, item_c;
		char				name_buf[512];
		GuestMem::Mapping::MapType	map_type;

		name_buf[0] = '\0';
		item_c = sscanf(
			buf,
			"%p-%p %d %d %s\n",
			(void**)&begin, (void**)&end, &prot,
			(int*)&map_type, name_buf);
		assert (item_c >= 4);

		if (prot == 0) continue;

		length =(uintptr_t)end - (uintptr_t)begin;

		snprintf(buf, BUFSZ, "%s/maps/%p", srcdir.c_str(), (void*)begin.o);
		fd = open(buf, O_RDONLY | O_CLOEXEC);
		assert (fd != -1);

		if (mem->is32Bit() && (begin.o > (1ULL << 32))) {
			fprintf(stderr,
				"[VEXLLVM] Snapshot ignoring %p\n",
				(void*)begin.o);
			continue;
		}

		if (	map_type == GuestMem::Mapping::VSYSPAGE &&
			Arch::getHostArch() == arch)
		{
			char	*sysp_buf = new char[length];
			ssize_t	sz;
			sz = read(fd, sysp_buf, length);
			assert (sz == (ssize_t)length);
			mem->addSysPage(begin, sysp_buf, length);
			mem->nameMapping(begin, name_buf);
			close(fd);
			continue;
		}

		if (map_type == GuestMem::Mapping::VSYSPAGE)
			map_type = GuestMem::Mapping::REG;

		int res = mem->mmap(mmap_addr,
			begin,
			length,
			prot,
			MAP_PRIVATE | MAP_FIXED,
			fd,
			0);

		if (res != 0) {
			fprintf(stderr, "Failed to addr=%p--%p\n",
				(void*)begin.o,
				(void*)(begin.o + length));
		}
		assert (res == 0 && "failed to map region on ss load");
		mem->setType(mmap_addr, map_type);
		mem->nameMapping(mmap_addr, name_buf);

		fd_list.push_back(fd);
	}

	END_F()
}

Symbols* GuestSnapshot::loadSymbols(const char* name)
{
	Symbols	*ret;

	ret = new Symbols();
	SETUP_F_R(name);

	do {
		unsigned int	elems;
		uint64_t	begin, end;

		elems = fscanf(
			f,
			"%" BUFSZ_STR "s %" PRIx64 "-%" PRIx64 "\n",
			buf, &begin, &end);
		if (elems != 3)
			break;
		ret->addSym(buf, begin, end - begin);
	} while (1);
	END_F();

	return ret;
}

GuestSnapshot::~GuestSnapshot(void)
{
	foreach (it, fd_list.begin(), fd_list.end()) {
		close(*it);
	}

	if (syms != NULL) delete syms;
	if (dyn_syms != NULL) delete dyn_syms;
}

#define SETUP_F_W(x)			\
	{ FILE		*f;		\
	char 		buf[BUFSZ];	\
	snprintf(buf, BUFSZ, "%s/%s", dirpath, x);	\
	f = fopen(buf, "w");				\
	assert (f != NULL && "failed to open "#x);
#define END_F()	fclose(f); }

static void saveThreads(const Guest* g, const char* dirpath)
{
	char		t_path[512];
	unsigned	thread_c(g->getNumThreads());

	/* no idle threads to save? */
	if (g->getNumThreads() == 0) return;

	/* create thread directory */
	snprintf(t_path, sizeof(t_path), "%s/threads", dirpath);
	mkdir(t_path, 0755);

	/* save all sleeping threads to thread directory */
	for (unsigned i = 0; i < thread_c; i++) {
		const GuestCPUState*	cpu(g->getThreadCPU(i+1));
		FILE			*f;
		ssize_t			wsz;

		snprintf(t_path, sizeof(t_path), "%s/threads/%d", dirpath, i);
		f = fopen(t_path, "w");
		assert (f != NULL);
		wsz = fwrite(cpu->getStateData(), cpu->getStateSize(), 1, f);
		assert (wsz == 1);
		fclose(f);
	}
}

static void saveSundries(const Guest* g, const char* dirpath)
{
	ssize_t	wsz;

	/* save binpath */
	SETUP_F_W("binpath")
	fputs(g->getBinaryPath(), f);
	END_F()

	/* save arch enum */
	SETUP_F_W("arch")
	Arch::Arch	a = g->getArch();
	wsz = fwrite(&a, sizeof(a), 1, f);
	assert (wsz == 1);
	END_F()

	/* save entry point */
	SETUP_F_W("entry")
	uint64_t	entry_pt = (uint64_t)g->getEntryPoint();
	wsz = fwrite(&entry_pt, sizeof(entry_pt), 1, f);
	assert (wsz == 1);
	END_F()

	/* save registers */
	SETUP_F_W("regs")
	const GuestCPUState	*cpu;
	cpu = g->getCPUState();
	wsz = fwrite(cpu->getStateData(), cpu->getStateSize(), 1, f);
	assert (wsz == 1);
	END_F()

	/* save argv pointers */
	SETUP_F_W("argv")
	std::vector<guest_ptr>	p(g->getArgvPtrs());
	foreach (it, p.begin(), p.end()) {
		fprintf(f, "%p\n", (void*)(it)->o);
	}
	END_F()

	if (g->getArgcPtr() != 0) {
		SETUP_F_W("argc");
		fprintf(f, "%p\n", (void*)g->getArgcPtr().o);
		END_F();
	}

	saveThreads(g, dirpath);
}

/* lame-o procfs */
/* XXX this isn't cross-endian, so be careful! */
void GuestSnapshot::save(const Guest* g, const char* dirpath)
{
	saveSundries(g, dirpath);
	saveMappings(g, dirpath);
	saveSymbols(g->getSymbols(), dirpath, "syms");
	saveSymbols(g->getDynSymbols(), dirpath, "dynsyms");
}

static void saveSymbolsDiff(
	const char* dirname, const char* last_dirname, const char* symfile)
{
	char	last_path[256], rl_path[256], target_path[256];
	ssize_t	rl_sz;
	int	err;


	sprintf(last_path, "%s/%s", last_dirname, symfile);
	sprintf(target_path, "%s/%s", dirname, symfile);
	rl_sz = readlink(last_path, rl_path, sizeof(rl_path));
	if (rl_sz == -1)
		strcpy(rl_path, last_path);
	else
		rl_path[rl_sz] = '\0';
	err = symlink(rl_path, target_path);
	(void)err;
}

void saveMappingsDiff(
	const Guest* g,
	const char* dirpath,
	const char* last_dirname,
	const std::set<guest_ptr>& changed_maps)
{
	/* save guestmem to mapinfo file */
	SETUP_F_W("mapinfo")

	/* force dir to exist */
	snprintf(buf, BUFSZ, "%s/maps", dirpath);
	mkdir(buf, 0755);

	/* add mappings */
	list<GuestMem::Mapping> maps = g->getMem()->getMaps();
	foreach (it, maps.begin(), maps.end()) {
		FILE			*map_f;
		GuestMem::Mapping	mapping(*it);
		ssize_t			sz;
		char			*buffer;
		char			last_buf[BUFSZ];
		struct stat		s;

		/* range, prot, type, name */
		fprintf(f, "%p-%p %d %d %s\n",
			(void*)mapping.offset.o,
			(void*)mapping.end().o,
			mapping.req_prot,
			(int)mapping.type,
			mapping.getName().c_str());

		snprintf(buf, BUFSZ, "%s/maps/%p",
			dirpath,
			(void*)mapping.offset.o);
		snprintf(last_buf, BUFSZ, "%s/maps/%p",
			last_dirname,
			(void*)mapping.offset.o);

		/* no change and exists in diff? setup symlink */
		if (	changed_maps.count(mapping.offset) == 0 &&
			stat(last_buf, &s) == 0)
		{
			char	real_p[BUFSZ];
			ssize_t	rl_sz;

			rl_sz = readlink(last_buf, real_p, sizeof(real_p));
			if (rl_sz == -1)
				strcpy(real_p, last_buf);
			else
				real_p[rl_sz] = '\0';
			if (symlink(real_p, buf) == 0)
				continue;
		}

		/* changed / doesn't exist in prior sshot, write out */
		map_f = fopen(buf, "w");
		assert (map_f && "Couldn't open mem range file");

		buffer = new char[mapping.length];
		g->getMem()->memcpy(buffer, mapping.offset, mapping.length);
		sz = fwrite(buffer, mapping.length, 1, map_f);
		delete [] buffer;
		assert (sz == 1 && "Failed to write mapping");

		fclose(map_f);
	}

	END_F()

}

void GuestSnapshot::saveDiff(
	const Guest* g,
	const char* dirname,
	const char* last_dirname,
	const std::set<guest_ptr>& changed_maps)
{
	char	full_last_dir[512];

	if (last_dirname[0] == '/') {
		strcpy(full_last_dir, last_dirname);
	} else {
		char	*r;
		r = getcwd(full_last_dir, 512);
		assert (r != NULL);
		strcat(full_last_dir, "/");
		strcat(full_last_dir, last_dirname);
	}

	saveSundries(g, dirname);
	saveMappingsDiff(g, dirname, full_last_dir, changed_maps);
	saveSymbolsDiff(dirname, full_last_dir, "syms");
	saveSymbolsDiff(dirname, full_last_dir, "dynsyms");
}

void GuestSnapshot::saveSymbols(
	const Symbols* g_syms,
	const char* dirpath,
	const char* name)
{
	SETUP_F_W(name);
	if (g_syms == NULL) goto done;

	foreach (it, g_syms->begin(), g_syms->end()) {
		const Symbol	*cur_sym = it->second;
		fprintf(f, "%s %" PRIx64 "-%" PRIx64 "\n",
			it->first.c_str(),
			(uint64_t)cur_sym->getBaseAddr(),
			(uint64_t)cur_sym->getEndAddr());
	}

done:
	END_F();
}

static void saveMappings(const Guest* g, const char* dirpath)
{
	/* save guestmem to mapinfo file */
	SETUP_F_W("mapinfo")

	/* force dir to exist */
	snprintf(buf, BUFSZ, "%s/maps", dirpath);
	mkdir(buf, 0755);

	/* add mappings */
	list<GuestMem::Mapping> maps = g->getMem()->getMaps();
	foreach (it, maps.begin(), maps.end()) {
		FILE			*map_f;
		GuestMem::Mapping	mapping(*it);
		ssize_t			sz;

		/* range, prot, type, name */
		fprintf(f, "%p-%p %d %d %s\n",
			(void*)mapping.offset.o,
			(void*)mapping.end().o,
			mapping.req_prot,
			(int)mapping.type,
			mapping.getName().c_str());

		snprintf(buf, BUFSZ, "%s/maps/%p", dirpath,
			(void*)mapping.offset.o);
		map_f = fopen(buf, "w");
		assert (map_f && "Couldn't open mem range file");

		const void *syspage_buf;
		syspage_buf = g->getMem()->getSysHostAddr(mapping.offset);
		if (!syspage_buf) {
			char* buffer = new char[mapping.length];
			g->getMem()->memcpy(buffer, mapping.offset, mapping.length);
			sz = fwrite(buffer, mapping.length, 1, map_f);
			delete [] buffer;
		} else {
			sz = fwrite(syspage_buf, mapping.length, 1, map_f);
		}

		assert (sz == 1 && "Failed to write mapping");

		fclose(map_f);
	}

	END_F()
}


char* GuestSnapshot::readMemory(
	const char* dirpath, guest_ptr p, unsigned int len)
{
	char			*read_mem;
	FILE			*f;
	size_t			sz;
	char			buf[256];
	DIR			*dir;
	struct dirent		*de;
	std::vector<uint64_t>	addrs;

	snprintf(buf, 256, "%s/maps", dirpath);
	dir = opendir(buf);
	assert (dir != NULL);
	while ((de = readdir(dir)) != NULL) {
		uint64_t	addr;
		sz = sscanf(de->d_name, "0x%" PRIx64, &addr);
		if (sz != 1)
			continue;
		addrs.push_back(addr);

	}
	closedir(dir);

	std::sort(addrs.begin(), addrs.end());
	uint64_t target_addr = 0;
	for (unsigned int i = 0; i < addrs[i]; i++) {
		if (addrs[i] > p.o)
			break;
		target_addr = addrs[i];
	}

	if (target_addr == 0)
		return NULL;

	snprintf(buf, 256, "%s/maps/0x%" PRIx64, dirpath, target_addr);
	f = fopen(buf, "rb");
	assert (f != NULL);
	if (fseek(f, p.o - target_addr, SEEK_SET) != 0) {
		fclose(f);
		return NULL;
	}
	read_mem = new char[len];
	sz = fread(read_mem, len, 1, f);
	assert (sz == 1);
	fclose(f);

	return read_mem;
}


bool GuestSnapshot::getPlatform(const char* plat_key, void* buf, unsigned len) const
{
	FILE	*f;
	char 	path[512];
	ssize_t br;

	sprintf(path, "%s/platform/%s", srcdir.c_str(), plat_key);
	f = fopen(path, "rb");
	if (f == NULL) {
		std::cerr << "[GuestSnapshot] Missing platform key '"
			<< plat_key << "'\n";
		return false;
	}

	if (buf != NULL) {
		br = fread(buf, len, 1, f);
		assert (br == 1);
	}

	fclose(f);

	return true;
}

void GuestSnapshot::loadThreads(void)
{
	ssize_t		sz;

	SETUP_F_R("regs")
	cpu_state = VexCPUState::create(arch);
	sz = fread(
		cpu_state->getStateData(),
		1,
		cpu_state->getStateSize(),
		f);
	if (sz != cpu_state->getStateSize()) {
		std::cerr << "Bad register size. Expected " <<
			cpu_state->getStateSize() << " got " <<
			sz << '\n';
	}
	assert (sz == cpu_state->getStateSize());
	END_F()

	SETUP_F_R_MAYBE("regs.ldt")
	if (f != NULL) {
		I386CPUState	*i386;
		char		*buf;
		int		res;
		guest_ptr	gp;

		i386 =  dynamic_cast<I386CPUState*>(cpu_state);
		assert (i386 != NULL && "ONLY I386 HAS LDT");
		buf = new char[8192*8];
		res = fread(buf, 8, 8192, f);
		assert (res == 8192 && "not enough LDT entries??");
		res = mem->mmap(gp, guest_ptr(0), 8192*8,
				PROT_WRITE | PROT_READ,
				MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		assert (gp.o && res == 0 && "failed to map ldt");

		mem->memcpy(gp, buf, 8192*8);
		i386->setLDT(gp);
		delete [] buf;
		END_F()
	}

	SETUP_F_R_MAYBE("regs.gdt")
	if (f != NULL) {
		I386CPUState	*i386;
		char		*buf;
		int		res;
		guest_ptr	gp;

		i386 =  dynamic_cast<I386CPUState*>(cpu_state);
		assert (i386 != NULL && "ONLY I386 HAS LDT");
		buf = new char[8192*8];
		res = fread(buf, 8, 8192, f);
		assert (res == 8192 && "not enough LDT entries??");
		res = mem->mmap(gp, guest_ptr(0), 8192*8,
				PROT_WRITE | PROT_READ,
				MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		assert (gp.o && res == 0 && "failed to map ldt");

		mem->memcpy(gp, buf, 8192*8);
		i386->setGDT(gp);
		delete [] buf;
		END_F()
	}

	/* TODO: load ldt/gdt for threads too */
	for (unsigned i = 0; ; i++) {
		char		fname[32];
		GuestCPUState	*cpu;

		snprintf(fname, 32, "threads/%d", i);
		SETUP_F_R_MAYBE(fname);
		if (f == NULL) break;
		cpu = VexCPUState::create(arch);
		sz = fread(cpu->getStateData(), 1, cpu->getStateSize(), f);
		thread_cpus.push_back(cpu);
		END_F();
	}
}
