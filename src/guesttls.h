#ifndef GUESTTLS_H
#define GUESTTLS_H

#include <stdint.h>

class GuestTLS
{
public:
	GuestTLS(bool in_delete_data = true);
	virtual ~GuestTLS(void);

	const uint8_t*		getBase(void) const { return tls_data; }
	uint8_t*		getBase(void) { return tls_data; }
	virtual unsigned int 	getSize(void) const;
protected:
	bool	delete_data;
	uint8_t	*tls_data;
};

#endif
