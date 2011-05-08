#include "Sugar.h"
#include "symbols.h"

Symbols::~Symbols()
{
	/* free all allocated symbols */
	foreach (it, name_map.begin(), name_map.end())
		delete (*it).second;
}

const Symbol* Symbols::findSym(const std::string& s) const
{
	symname_map::const_iterator it;

	it = name_map.find(s);
	if (it == name_map.end()) return NULL;

	return (*it).second;
}

/* TODO: this should be a range query so we can detect
 * mid-symbol accesses. */
const Symbol* Symbols::findSym(void* ptr) const
{
	symaddr_map::const_iterator it;

	it = addr_map.find((symaddr_t)ptr);
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