#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include "Sugar.h"
#include "guestcpustate.h"
#include "guestsnapshot.h"

using namespace std;

#ifdef __amd64__
#define SYSPAGE_ADDR	guest_ptr(0xffffffffff600000)
#endif

GuestSnapshot* GuestSnapshot::create(GuestMem* mem, const char* dirpath)
{
	GuestSnapshot	*ret;

	ret = new GuestSnapshot(mem, dirpath);
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

GuestSnapshot::GuestSnapshot(GuestMem* mem, const char* dirpath)
: Guest(mem, NULL), is_valid(false)
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
	cpu_state = GuestCPUState::create(mem, arch);
	sz = fread(
		cpu_state->getStateData(),
		cpu_state->getStateSize(),
		1,
		f);
	assert (sz == 1);
	END_F()

	/* setup guestmem */
	SETUP_F_R("mapinfo")
	assert (mem == NULL);
	mem = new GuestMem();

	while (fgets(buf, 512, f) != NULL) {
		guest_ptr	begin, end, mmap_addr;
		size_t		length;
		int		prot, is_stack;
		int		item_c;
		int		fd;

		item_c = sscanf(
			buf,
			"%p-%p %d %d\n",
			(void**)&begin, (void**)&end, &prot, &is_stack);
		assert (item_c == 4);

		length =(uintptr_t)end - (uintptr_t)begin;

		snprintf(buf, 512, "%s/maps/%p", dirpath, (void*)begin.o);
		fd = open(buf, O_RDONLY);
		assert (fd != -1);
		int res = mem->mmap(mmap_addr,
			begin,
			length,
			prot,
			MAP_PRIVATE | MAP_FIXED,
			fd,
			0);
		assert (res == 0 && "failed to map region on ss load");
		fd_list.push_back(fd);
	}

#ifdef __amd64__
	/* cheat so sys page shows up even though we skip it on save */
	GuestMem::Mapping	sys_page(
		SYSPAGE_ADDR, 4096, PROT_READ | PROT_EXEC);
	mem->recordMapping(sys_page);
#endif
	END_F()

	is_valid = true;
}

GuestSnapshot::~GuestSnapshot(void)
{
	foreach (it, fd_list.begin(), fd_list.end()) {
		close(*it);
	}
	/* nothing yet */
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

	/* save guestmem */
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

#ifdef __amd64__
		/* XXX stupid hack for kernel page; arch specific */
		/* would like to write it out, but
		 * 1. fwrite will fail it since it's kernel space addr
		 * 2. it'll already be mapped on load()
		 *
		 * If we ever get cross platform snapshots, it'll 
		 * need to actually be written out with a bounce buffer.
		 */
		if (mapping.offset == SYSPAGE_ADDR)
			continue;
#endif

		/* range, prot, is_stack */
		fprintf(f, "%p-%p %d %d\n",
			(void*)mapping.offset.o,
			(void*)mapping.end().o,
			mapping.req_prot,
			(int)(mapping.isStack()));

		snprintf(buf, 512, "%s/maps/%p", dirpath,
			(void*)mapping.offset.o);
		map_f = fopen(buf, "w");
		assert (map_f && "Couldn't open mem range file");

		char* buffer = new char[mapping.length];
		g->getMem()->memcpy(buffer, mapping.offset, mapping.length);
		sz = fwrite(buffer, mapping.length, 1, map_f);
		delete [] buffer;
		assert (sz == 1 && "Failed to write mapping");
		fclose(map_f);

	}

	END_F()
	/* done saving map */
}
