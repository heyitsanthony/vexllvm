#ifndef RES_USAGE_H
#define RES_USAGE_H

#include <sys/resource.h>

/* compute resource changes since construction of object */
class ResUsage {
public:
	ResUsage(const std::string& n = "");
	~ResUsage();
	void dump_diff(void) const;
private:
	std::string	name;
	struct rusage	init_usage;
};

#endif
