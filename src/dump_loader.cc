#include <iostream>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include "guestptimg.h"
#include <errno.h>
#include <fstream>
#include <sstream>

void dumpIRSBs(void) {}

static bool loadEntry(
	GuestMem* mem,
	pid_t pid,
	const char* save_arg,
	std::istream& is)
{
	char		line_buf[256];

	is.get(line_buf, 256, '\n');
	if(is.fail()) return false;
	is.get();

	PTImgMapEntry m(mem, pid, line_buf);

	std::cerr << m.getBase()
		<< " sz: " << (void*)m.getByteCount()
		<< " prot: " << std::hex << m.getProt()
		<< " lib: " << m.getLib()
		<< std::endl;

	std::ostringstream save_fname;
	save_fname << save_arg << "." << pid << "." << m.getBase();

	char* buffer = (char*)malloc(m.getByteCount());
	for(unsigned int i = 0; i < m.getByteCount(); i += sizeof(long)) {
		*(long*)&buffer[i] = ptrace(
			PTRACE_PEEKTEXT, pid, m.getBase().o + i, NULL);
	}
	std::ofstream o(save_fname.str().c_str());
	o.write(buffer, m.getByteCount());
	free(buffer);

	return true;
}

int main(int argc, char *argv[], char *envp[])
{
	if(argc < 2) {
		std::cerr << "Usage: " << argv[0]
			<< " prog args ..." << std::endl;
		exit(1);
	}

	pid_t pid = fork();
	int err;
	if(!pid) {
		ptrace(PTRACE_TRACEME, 0, NULL, NULL);
		char * env[] = { NULL };
		err = execvpe(argv[1], &argv[1], env);
		assert (err != -1 && "EXECVE FAILED. NO PTIMG!");
	}
	wait(NULL);
	ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
	wait(NULL);

	std::ostringstream map_fname;
	map_fname << "/proc/" << pid << "/maps";
	
	GuestMem* mem = new GuestMem();
	std::ifstream i(map_fname.str().c_str());
	while (loadEntry(mem, pid, argv[1], i));

	kill(pid, SIGKILL);
	return 0;
}
