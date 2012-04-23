#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "procargs.h"

static int splitFile(const char* fname, char* argv[]);
static int splitToBuf(char *buf, int buf_sz, char* argv[]);

ProcArgs::ProcArgs(int _pid)
: pid(_pid)
{
	char	fname[128];

	memset(argv, 0, sizeof(argv));
	memset(env, 0, sizeof(env));
	sprintf(fname, "/proc/%d/cmdline", pid);
	argc = splitFile(fname, argv);
	assert (argc > -1);

	sprintf(fname, "/proc/%d/environ", pid);
	envc = splitFile(fname, env);
	assert (envc > -1);
}

ProcArgs::~ProcArgs(void)
{
	for (int i = 0; i < argc; i++)
		if (argv[i])
			free(argv[i]);

	for (int i = 0; i < envc; i++)
		if (env[i])
			free(env[i]);
}

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
	char	argv_dat[DAT_MAX];

	f = fopen(fname, "r");
	if (f == NULL)
		return -1;

	sz = fread(argv_dat, 1, DAT_MAX, f);
	assert (sz >= 0);
	argv_dat[sz] = '\0';

	fclose(f);

	return splitToBuf(argv_dat, sz, argv);
}
