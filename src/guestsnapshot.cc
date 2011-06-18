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
: Guest(NULL), is_valid(false)
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

	/* setup guestmem */
	SETUP_F_R("mapinfo")
	assert (mem == NULL);
	mem = new GuestMem();

	while (fgets(buf, 512, f) != NULL) {
		void		*begin, *end, *mmap_addr;
		size_t		length;
		int		prot, is_stack;
		int		item_c;
		int		fd;

		item_c = sscanf(
			buf,
			"%p-%p %d %d\n",
			&begin, &end, &prot, &is_stack);
		assert (item_c == 4);

		length =(uintptr_t)end - (uintptr_t)begin;

		snprintf(buf, 512, "%s/maps/%p", dirpath, begin);
		fd = open(buf, O_RDONLY);
		assert (fd != -1);
		mmap_addr = mmap(
			begin,
			length,
			prot,
			MAP_PRIVATE | MAP_FIXED,
			fd,
			0);
		assert (mmap_addr != MAP_FAILED);
		fd_list.push_back(fd);

		GuestMem::Mapping	s(begin, length, prot,(is_stack != 0));
		mem->recordMapping(s);
	}
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
		#define BAD_OFFSET_MASK	0xffffffffff000000
		FILE			*map_f;
		GuestMem::Mapping	mapping(*it);
		ssize_t			sz;

		/* XXX stupid hack, probably arch specific */
		if (((uintptr_t)mapping.offset & BAD_OFFSET_MASK)
			== BAD_OFFSET_MASK)
		{
			continue;
		}

		/* range, prot, is_stack */
		fprintf(f, "%p-%p %d %d\n",
			mapping.offset,
			mapping.end(),
			mapping.req_prot,
			(int)(mapping.isStack()));

		snprintf(buf, 512, "%s/maps/%p", dirpath, mapping.offset);
		map_f = fopen(buf, "w");
		sz = fwrite(mapping.offset, mapping.length, 1, map_f);
		assert (sz == 1 && "Failed to write mapping");
		fclose(map_f);

	}

	END_F()
	/* done saving map */
}
