#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "util.h"
#include "symbols.h"
#include "guestcpustate.h"
#include "guestsnapshot.h"

#include "guest.h"

#include "elfimg.h"
#include "elfdebug.h"

Guest::Guest(const char* in_bin_path)
: cpu_state(NULL),
  mem(NULL),
  bin_path(NULL)
{
	setBinPath(in_bin_path);
}

Guest::~Guest(void)
{
	if (bin_path) free(bin_path);
	if (mem) delete mem;
	if (cpu_state) delete cpu_state;
}

void Guest::setBinPath(const char* p)
{
	if (bin_path) free(bin_path);
	bin_path = NULL;
	if (p == NULL) return;
	bin_path = strdup(p);
}

SyscallParams Guest::getSyscallParams(void) const
{
	return cpu_state->getSyscallParams();
}

void Guest::setSyscallResult(uint64_t ret)
{
	cpu_state->setSyscallResult(ret);
}

std::string Guest::getName(guest_ptr x) const
{
	const Symbol	*sym_found;
	const Symbols	*syms;

	syms = getSymbols();
	if (syms == NULL)
		return hex_to_string(x);

	sym_found = syms->findSym(x);
	if (sym_found != NULL) {
		return (sym_found->getName()+"+")+
			hex_to_string((intptr_t)x-sym_found->getBaseAddr());
	}

	return hex_to_string(x); 
}

uint64_t Guest::getExitCode(void) const
{
	return cpu_state->getExitCode();
}

void Guest::print(std::ostream& os) const { cpu_state->print(os); }

std::list<GuestMem::Mapping> Guest::getMemoryMap(void) const
{
	assert (mem != NULL);
	return mem->getMaps();
}

#define LAST_SYMLINK	"guest-last"

void Guest::save(const char* dirpath) const
{
	int		err;
	char		buf[512];

	if (dirpath == NULL) {
		/* make up a directory name */
		struct timeval	tv;
		err = gettimeofday(&tv, NULL);
		assert(err==0);
		snprintf(buf,
			512,
			"guest-%d.%03d",
			(int32_t)tv.tv_sec,	/* FIXME by 2038 */
			(int32_t)tv.tv_usec/1000 /* ms */);
		dirpath = buf;
	}

	/* always write something new to avoid accidental overwrites */
	err = mkdir(dirpath, 0755);
	assert (err != -1 && "Could not make save directory");

	/* unlink symlink to last guest, if any */
	unlink(LAST_SYMLINK);

	/* link symlink to new guest */
	err = symlink(dirpath, LAST_SYMLINK);

	/* dump it */
	GuestSnapshot::save(this, dirpath);
}

Guest* Guest::load(const char* dirpath)
{
	GuestSnapshot*	ret;

	/* use most recent dir if don't care */
	if (dirpath == NULL) dirpath = LAST_SYMLINK;

	ret = GuestSnapshot::create(dirpath);
	return ret;
}


void Guest::addLibrarySyms(const char* path, guest_ptr base)
{
	Symbols	*cur_syms = getSymbols();
	if (cur_syms == NULL)
		return;
	addLibrarySyms(path, base, cur_syms);
}

void Guest::addLibrarySyms(const char* path, guest_ptr base, Symbols* cur_syms)
{
	Symbols*	new_syms;

	new_syms = ElfDebug::getSyms(path, base);
	if (new_syms == NULL)
		return;

	cur_syms->addSyms(new_syms);
	delete new_syms;
}


void Guest::toCore(const char* path) const
{
	std::ofstream	os(((path) ? path : "core"));

	if (!os.good()) {
		std::cerr << "[Guest] Couldn't save core.\n";
		return;
	}

	ElfImg::writeCore(this, os);
}
