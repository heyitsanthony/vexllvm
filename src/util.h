#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <string>
#include <sstream>

#define iter_do(x,y,z)	\
	do {	\
		for (	x::const_iterator it = (y).begin(); 	\
			it != (y).end();			\
			it++) {					\
				(*it)->z();			\
			}					\
	} while (0)

static inline std::string int_to_string(uint64_t i)
{
	std::string s;
	std::stringstream out;
	out << i;
	return out.str();
}

static char hexmap[] = {"0123456789abcdef"};

static inline std::string hex_to_string(uint64_t i)
{
	std::string s;
	std::stringstream out;

	out << "0x";
	while (i) {
		out << hexmap[(i & 0xf0) >> 4] << hexmap[(i & 0xf)];
		i >>= 8;
	}
	return out.str();
}

static inline unsigned long getLongHex(const char* s)
{
	unsigned long 	ret;
	int		i;

	ret = 0;
	for (i = 0; s[i]; i++) {
		unsigned char	c = s[i];

		ret <<= 4;
		if (c >= 'A' && c <= 'F') {
			c -= 'A';
			c += 'a';
		}

		if (c >= 'a' && c <= 'f')
			ret |= (c - 'a') + 10;
		else
			ret |= c - '0';
	}

	return ret;
}

#endif
