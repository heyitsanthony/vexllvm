#ifndef PROCARGS_H
#define PROCARGS_H

//
// Do us a favor and use a real algorithm.
// -- Sun Engineer
#define ARGV_MAX	64
#define DAT_MAX		8192	/* env on my desktop is 5.6k; 3.6k on fogger */

class ProcArgs
{
public:
	ProcArgs(int pid);
	virtual ~ProcArgs(void);
	int getArgc(void) const { return argc; }
	char** getArgv(void) { return argv; }
	char** getEnv(void) { return env; }
private:
	int	pid;
	int	argc;
	int	envc;
	char	*argv[ARGV_MAX];
	char	*env[ARGV_MAX];
};

#endif
