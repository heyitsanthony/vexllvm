#include <assert.h>
#include <iostream>
#include "usage.h"

ResUsage::ResUsage(const std::string& n)
: name(n)
{
	int rc = getrusage(RUSAGE_SELF, &init_usage);
	assert(rc == 0);
}

void ResUsage::dump_diff(void) const
{
	struct rusage cur_usage;
	int rc = getrusage(RUSAGE_SELF, &cur_usage);
	assert(rc == 0);
	int64_t	diff = cur_usage.ru_maxrss - init_usage.ru_maxrss;
	if (!name.empty()) std::cerr << '[' << name << "] ";
	std::cerr << "RSS: " << ((double)diff)/1024.0 << "MB\n";
}

ResUsage::~ResUsage()
{
	dump_diff();
}
