#ifndef SYMBOLS_H
#define SYMBOLS_H

#include <stdint.h>
#include <map>
#include <string>

typedef uint64_t symaddr_t;

class Symbol
{
public:
	Symbol(	const std::string& in_name, symaddr_t in_addr,
		unsigned int in_len)
	: name(in_name), base_addr(in_addr), length(in_len) {}
	virtual ~Symbol() {}
	const std::string& getName() const { return name; }
	symaddr_t getBaseAddr() const { return base_addr; }
	unsigned int getLength() const { return length; }
private:
	std::string	name;
	symaddr_t	base_addr;
	unsigned int	length;
};

/* takes a symbol of form (name, address start, size).
 * replies to questions about which symbol contains a given address
 * replies to questions about which name maps to a symbol
 * **assumes no overlaps **
 */

typedef std::map<std::string, Symbol*> symname_map;
typedef std::map<symaddr_t, Symbol*> symaddr_map;

class Symbols
{
public:
	Symbols() {}
	virtual ~Symbols();
	const Symbol* findSym(const std::string& s) const;
	const Symbol* findSym(void* ptr) const;
	bool addSym(
		const std::string& name,
		symaddr_t addr,
		unsigned int len);
	void addSym(const Symbol* sym);
	void addSyms(const Symbols* syms);
private:
	symname_map	name_map;
	symaddr_map	addr_map;
};

#endif
