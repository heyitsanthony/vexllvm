#include "Sugar.h"

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>


#include "elfimg.h"
#include "vexexec.h"
#include "guestcpustate.h"
#include "guestptimg.h"

using namespace llvm;

static VexExec *vexexec;

void dumpIRSBs(void)
{
	std::cerr << "DUMPING LOGS" << std::cerr;
	vexexec->dumpLogs(std::cerr);
}

//
// Do us a favor and use a real algorithm.
// -- Sun Engineer
#define ARGV_MAX	32
#define DAT_MAX		1024

static int splitToBuf(char *buf, int buf_sz, char* argv[])
{
	int argc = 0;
	argv[0] = strdup(buf);
	for (argc = 1; argc < ARGV_MAX; argc++) {
		char	*s, *next_s;
		int	len;

		argv[argc] = NULL;
		s = argv[argc-1];
		len = strlen(s);
		if (len == 0)
			break;

		next_s = len + 1 + s;
		if ((uintptr_t)next_s >= (uintptr_t)(buf + buf_sz)) {
			break;
		}
		argv[argc] = strdup(next_s);
	}

	argv[argc] = NULL;
	return argc;
}

static int splitFile(const char* fname, char* argv[])
{
	FILE	*f;
	size_t	sz;
	char	argv_dat[1024];

	f = fopen(fname, "r");
	if (f == NULL)
		return -1;

	sz = fread(argv_dat, 1, DAT_MAX, f);
	fclose(f);

	return splitToBuf(argv_dat, sz, argv);
}

GuestPTImg* createAttached(void)
{
	static char	*argv[ARGV_MAX];
	static char	*env[ARGV_MAX];
	int	pid;
	char	fname[256];
	int	argc, envc;

	pid = atoi(getenv("VEXLLVM_ATTACH"));
	sprintf(fname, "/proc/%d/cmdline", pid);
	argc = splitFile(fname, argv);
	if (argc == -1)
		return NULL;

	sprintf(fname, "/proc/%d/environ", pid);
	envc = splitFile(fname, env);
	if (envc == -1)
		return NULL;

	return GuestPTImg::createAttached<GuestPTImg>(pid, argc, argv, env);
}

int main(int argc, char* argv[], char* envp[])
{
	GuestPTImg	*gs;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s program_path <args>\n", argv[0]);
		return -1;
	}

	if (getenv("VEXLLVM_ATTACH") != NULL) {
		gs = createAttached();
	} else {
		gs = GuestPTImg::create<GuestPTImg>(argc - 1, argv + 1, envp);
	}
	vexexec = VexExec::create<VexExec,Guest>(gs);
	assert (vexexec && "Could not create vexexec");
	
	vexexec->run();

	delete vexexec;
	delete gs;

	return 0;
}
