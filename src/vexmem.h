#ifndef VEXMEM_H
#define VEXMEM_H

#include <map>

class VexMem
{
public:
	typedef struct {
		void* offset;
		size_t length;
		int req_prot;
		int cur_prot;
		void* end() const { return (char*)offset + length; }
	} Mapping;

	VexMem(void);
	virtual ~VexMem(void);
	void recordMapping(Mapping& mapping);
	void removeMapping(Mapping& mapping);
	bool lookupMapping(void* addr, Mapping& mapping);
	void* brk();
	bool sbrk(void* new_top);
private:
	typedef std::map<void*, Mapping> mapmap_t; 
	mapmap_t maps;
	void* top_brick;
};

#endif
