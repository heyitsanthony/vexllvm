/* list collection that will delete all pointers when it is deleted */
#ifndef COLLECTION_H
#define COLLECTION_H

#include <assert.h>

#include <vector>
#include <list>

template <typename T>
class PtrList : public std::list<T*>
{
public:
	PtrList() {}
	PtrList(const PtrList<T>& pl)
	{
		typename std::list<T*>::const_iterator	it;
		for (it = pl.begin(); it != pl.end(); it++) add((*it)->copy());
	}

	virtual ~PtrList()
	{
		typename std::list<T*>::iterator	it;
		for (it = this->begin(); it != this->end(); it++) delete (*it);
	}
	virtual void add(T* t) { assert (t != ((T*)0)); this->push_back(t); }
	virtual void clear_nofree(void)
	{
		std::list<T*>::clear();
	}

	virtual void clear(void)
	{
		typename std::list<T*>::iterator	it;
		for (it = this->begin(); it != this->end(); it++) delete (*it);
		std::list<T*>::clear();
	}
};

#endif
