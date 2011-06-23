#include "Sugar.h"
#include <stdio.h>
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

const Symbol* Symbols::findSym(void* ptr) const
{
	symaddr_map::const_iterator it;

	it = addr_map.lower_bound((symaddr_t)ptr);
	if (it == addr_map.end()) return NULL;

	return (*it).second;
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