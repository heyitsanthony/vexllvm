#include "Sugar.h"
#include <stdio.h>
#include <assert.h>
#include "symbols.h"

Symbols::~Symbols()
{
	/* free all allocated symbols */
	foreach (it, addr_map.begin(), addr_map.end())
		delete (*it).second;
}

const Symbol* Symbols::findSym(const std::string& s) const
{
	symname_map::const_iterator it;

	it = name_map.find(s);
	if (it == name_map.end()) return NULL;

	return (*it).second;
}

const Symbol* Symbols::findSym(uint64_t ptr) const
{
	const Symbol			*ret;
	symaddr_map::const_iterator	it;

	if (ptr == 0)
		return NULL;

	it = addr_map.upper_bound((symaddr_t)ptr-1);
	if (it == addr_map.end())
		return NULL;

	ret = it->second;
	if (ret->getBaseAddr() == (symaddr_t)ptr)
		return ret;

	--it;
	if (it == addr_map.end())
		return NULL;

	ret = it->second;
	assert (ret->getBaseAddr() <= (symaddr_t)ptr && "WTF");

	return ret;
}

bool Symbols::addSym(const std::string& name, symaddr_t addr, unsigned int len)
{
	Symbol	*sym;

	if (addr_map.count(addr)) return false;
	if (name_map.count(name)) return false;

	sym  = new Symbol(name, addr, len);
	addr_map[addr] = sym;
	name_map[name] = sym;

	return true;
}

void Symbols::addSym(const Symbol* sym)
{
	addSym(sym->getName(), sym->getBaseAddr(), sym->getLength());
}

void Symbols::addSyms(const Symbols* syms)
{
	foreach (it, syms->name_map.begin(), syms->name_map.end())
		addSym(it->second);
}