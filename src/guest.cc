#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "util.h"
#include "guestcpustate.h"
#include "guestsnapshot.h"

#include "guest.h"


using namespace llvm;

Guest::Guest(const char* in_bin_path)
: mem(NULL),
  bin_path(NULL)
{
	setBinPath(in_bin_path);
}

Guest::~Guest(void)
{
	if (bin_path) free(bin_path);

	if (mem) {
		delete mem;
		mem = NULL;
	}

	delete cpu_state;
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

std::string Guest::getName(void* x) const { 
	return hex_to_string((uintptr_t)x); 
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
