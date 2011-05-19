#include "guesttls.h"
#include <string.h>

#define TLS_DATA_SIZE	(64*1024) /* XXX. Good enough? */

GuestTLS::GuestTLS(bool in_delete_data)
: delete_data(in_delete_data)
{
	uint64_t	ptr_guard;

	tls_data = new uint8_t[TLS_DATA_SIZE];
	memset(tls_data, 0, TLS_DATA_SIZE);

	/* XXX pointer guard. LD uses the same pointer guard as the host
	 * process (luckily). We really need our own LD to get around this
	 * for other hosts. Alternative: force env LD_POINTER_GUARD=0 */
	__asm__("movq %%fs:0x30, %0" : "=r" (ptr_guard));
	*((uint64_t*)(&tls_data[0x30])) = ptr_guard;

	/* and a fake pthreads structure?? */
	*((uint64_t*)(&tls_data[0])) = (uint64_t)tls_data;
}

GuestTLS::~GuestTLS(void)
{
	if (delete_data) delete [] tls_data;
}

unsigned int GuestTLS::getSize(void) const { return TLS_DATA_SIZE; }
