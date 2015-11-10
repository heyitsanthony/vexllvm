#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "arch.h"
#include "guestcpustate.h"
#include "guestabi.h"
#include "guestfragment.h"

std::string GuestFragment::getName(guest_ptr gp) const
{
	return "frag_" + Guest::getName(gp);
}

GuestFragment* GuestFragment::fromFile(
	const char* 	fname,
	Arch::Arch	arch,
	guest_ptr 	gp_base)
{
	struct stat	s;
	FILE		*f;
	char		*buf;
	int		rc;
	size_t		br;
	GuestFragment	*ret = NULL;
	
	f = fopen(fname, "r");
	if (f == NULL) goto err_file;

	rc = fstat(fileno(f), &s);
	if (rc == -1) goto err_stat;
	if (s.st_size == 0) goto err_stat;

	buf = new char[s.st_size];
	br = fread(buf, s.st_size, 1, f);
	if (br != 1) goto err_read;

	ret = new GuestFragment(arch, buf, gp_base, s.st_size);

err_read:
	delete [] buf;
err_stat:
	fclose(f);
err_file:
	return ret;
}

GuestFragment::~GuestFragment(void)
{
	munmap(code, getCodePageBytes());
}

GuestFragment::GuestFragment(
	Arch::Arch in_arch,
	const char* data,
	guest_ptr in_base,
	unsigned int in_len)
: Guest("fragment")
, arch(in_arch)
, guest_base(in_base)
, code_len(in_len)
{
	assert (code_len > 0 && "Creating empty fragment!!!");
	code = (char*)mmap(
		(void*)guest_base.o,
		getCodePageBytes(),
		PROT_READ | PROT_WRITE,
		MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED,
		-1,
		0);
	assert (code != MAP_FAILED && "Could not map fragment");
	memcpy(code, data, code_len);

	cpu_state = GuestCPUState::create(arch);
	abi = GuestABI::create(this);

	/* our one fragment mapping */
	mem = new GuestMem();
	GuestMem::Mapping m(in_base, in_len, PROT_READ | PROT_EXEC);
	mem->recordMapping(m);
}
