#include <errno.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/mman.h>

#include <sstream>

#include "Sugar.h"
#include "syscall/syscallsmarshalled.h"
#include "ptimgremote.h"
#include "guestmemsink.h"
#include "guestptmem.h"
#include "guestcpustate.h"

/*
 * single step shadow program while counter is in specific range
 */
PTImgRemote::PTImgRemote(const char* binname, bool use_entry)
: GuestPTImg(binname, use_entry)
, PTCtl((GuestPTImg&)*this)
, blocks(0)
{}

PTImgRemote::~PTImgRemote() {}

/* replace physically backed memory with remote memory */
void PTImgRemote::slurpBrains(pid_t pid)
{
	GuestMem	*tmp_mem;
	bool		is_32;

	/* XXX this is dumb, be smarter about sizes */
	is_32 = mem->is32Bit();

	delete mem;
	mem = new GuestMemSink();

	/* read in with sink guestmem */
	std::cerr << "[PTImgRemote] Slurp brains\n";
	ProcMap::slurpMappings(pid, mem, mappings, false);
	slurpRegisters(pid);

	tmp_mem = new GuestPTMem(this, pid);
	if (is_32) tmp_mem->mark32Bit();
	tmp_mem->import(mem);

	std::cerr << "[PTImgRemote] Imported memory map\n";

	delete mem;
	mem = tmp_mem;
}

void PTImgRemote::handleChild(pid_t pid)
{
	/* keep child around */
	setPID(pid);
}
