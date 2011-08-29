#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include "Sugar.h"
#include "symbols.h"
#include "util.h"
#include "guestcpustate.h"
#include "guestsnapshot.h"

using namespace std;

#ifdef __amd64__
#define SYSPAGE_ADDR	guest_ptr(0xffffffffff600000)
#endif

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

#define SETUP_F_R(x)			\
	{ FILE		*f;		\
	char 		buf[512];	\
	snprintf(buf, 512, "%s/%s", dirpath, x);	\
	f = fopen(buf, "r");				\
	assert (f != NULL && "failed to open "#x);
#define END_F()	fclose(f); }

GuestSnapshot::GuestSnapshot(const char* dirpath)
: Guest(NULL)
, is_valid(false)
, syms(NULL)
{
	ssize_t	sz;

	SETUP_F_R("binpath")
	const char	*fget_buf;
	fget_buf = fgets(buf, 512, f);
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

	SETUP_F_R("regs")
	cpu_state = GuestCPUState::create(arch);
	sz = fread(
		cpu_state->getStateData(),
		cpu_state->getStateSize(),
		1,
		f);
	assert (sz == 1);
	END_F()

	SETUP_F_R("argv")
	argv_ptrs.clear();
	uint64_t	p;
	while (fscanf(f, "%p\n", ((void**)&p)) == 1) {
		argv_ptrs.push_back(guest_ptr(p));
	}
	END_F()

	loadMappings(dirpath);
	loadSymbols(dirpath);

	is_valid = true;
}

void GuestSnapshot::loadMappings(const char* dirpath)
{
	SETUP_F_R("mapinfo")
	assert (mem == NULL);
	mem = new GuestMem();

	while (fgets(buf, 512, f) != NULL) {
		guest_ptr			begin, end, mmap_addr;
		size_t				length;
		int				prot, fd, item_c;
		GuestMem::Mapping::MapType	map_type;

		item_c = sscanf(
			buf,
			"%p-%p %d %d\n",
			(void**)&begin, (void**)&end, &prot, (int*)&map_type);
		assert (item_c == 4);

		length =(uintptr_t)end - (uintptr_t)begin;

		snprintf(buf, 512, "%s/maps/%p", dirpath, (void*)begin.o);
		fd = open(buf, O_RDONLY);
		assert (fd != -1);

		if (map_type == GuestMem::Mapping::VSYSPAGE) {
			char	*sysp_buf = new char[length];
			ssize_t	sz;
			sz = read(fd, sysp_buf, length);
			assert (sz == length);
			mem->addSysPage(guest_ptr(begin), sysp_buf, length);
			close(fd);
			continue;
		}

		int res = mem->mmap(mmap_addr,
			begin,
			length,
			prot,
			MAP_PRIVATE | MAP_FIXED,
			fd,
			0);

		assert (res == 0 && "failed to map region on ss load");
		mem->setType(mmap_addr, map_type);

		fd_list.push_back(fd);
	}

	END_F()
}

void GuestSnapshot::loadSymbols(const char* dirpath)
{
	syms = new Symbols();
	SETUP_F_R("syms");

	do {
		unsigned int	elems;
		uint64_t	begin, end;

		elems = fscanf(f, "%s %"PRIx64"-%"PRIx64"\n", buf, &begin, &end);
		if (elems != 3)
			break;
		syms->addSym(buf, begin, end - begin);
	} while (1);
	END_F();
}

GuestSnapshot::~GuestSnapshot(void)
{
	foreach (it, fd_list.begin(), fd_list.end()) {
		close(*it);
	}

	if (syms != NULL) delete syms;
}

#define SETUP_F_W(x)			\
	{ FILE		*f;		\
	char 		buf[512];	\
	snprintf(buf, 512, "%s/%s", dirpath, x);	\
	f = fopen(buf, "w");				\
	assert (f != NULL && "failed to open "#x);
#define END_F()	fclose(f); }

/* lame-o procfs */
/* XXX this isn't cross-endian, so be careful! */
void GuestSnapshot::save(const Guest* g, const char* dirpath)
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

	saveMappings(g, dirpath);
	saveSymbols(g, dirpath);
}

void GuestSnapshot::saveSymbols(const Guest* g, const char* dirpath)
{
	const Symbols	*g_syms;

	SETUP_F_W("syms");
	g_syms = g->getSymbols();
	if (g_syms == NULL) goto done;

	foreach (it, g_syms->begin(), g_syms->end()) {
		const Symbol	*cur_sym = it->second;
		fprintf(f, "%s %"PRIx64"-%"PRIx64"\n",
			it->first.c_str(),
			cur_sym->getBaseAddr(),
			cur_sym->getEndAddr());
	}

done:
	END_F();
}

void GuestSnapshot::saveMappings(const Guest* g, const char* dirpath)
{
	/* save guestmem to mapinfo file */
	SETUP_F_W("mapinfo")

	/* force dir to exist */
	snprintf(buf, 512, "%s/maps", dirpath);
	mkdir(buf, 0755);

	/* add mappings */
	list<GuestMem::Mapping> maps = g->getMem()->getMaps();
	foreach (it, maps.begin(), maps.end()) {
		FILE			*map_f;
		GuestMem::Mapping	mapping(*it);
		ssize_t			sz;

		/* range, prot, type */
		fprintf(f, "%p-%p %d %d\n",
			(void*)mapping.offset.o,
			(void*)mapping.end().o,
			mapping.req_prot,
			(int)mapping.type);

		snprintf(buf, 512, "%s/maps/%p", dirpath,
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
