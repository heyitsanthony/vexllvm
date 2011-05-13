#ifndef GUESTTLS_H
#define GUESTTLS_H

#include <stdint.h>

class GuestTLS
{
public:
	GuestTLS(void);
	virtual ~GuestTLS(void);

	const uint8_t*	getBase(void) const { return tls_data; }
	uint8_t*	getBase(void) { return tls_data; }
	unsigned int 	getSize(void) const;
private:
	uint8_t	*tls_data;
};

#endif
