#ifndef DIRECTCACHE_H
#define DIRECTCACHE_H

#include <stdint.h>
#include <string.h>

#define DC_SIZE 4096

template <typename T>
class DirectCache
{
public:
	DirectCache(void)
	{
		memset(ents, 0, sizeof(ents));
	}

	virtual ~DirectCache() {}

	void put(void* p, T* t)
	{
		unsigned int	idx;
		idx = ptr2slot(p);
		ents[idx].ptr = p;
		ents[idx].t = t;
	}

	T* get(void* p) const
	{
		unsigned int	idx;
		idx = ptr2slot(p);
		if (ents[idx].ptr == p) return ents[idx].t;
		return NULL;
	}

	void flush(void) { memset(ents, 0, sizeof(ents)); }
private:
	unsigned int ptr2slot(void* p) const
	{
		uintptr_t	n;
		n = (((uintptr_t)p) >> 2); /* who cares about the lowest bits */
		return n % DC_SIZE;
	}

	struct dc_pair
	{
		void*	ptr;
		T*	t;
	};

	struct dc_pair	ents[DC_SIZE];
};

#endif
