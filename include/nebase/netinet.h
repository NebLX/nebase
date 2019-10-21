
#ifndef NEB_NETINET_H
#define NEB_NETINET_H 1

#include "cdefs.h"

#include <arpa/inet.h>

#define NEB_INET_ARPASTRLEN (INET_ADDRSTRLEN+1+12) // rfc1035
#define NEB_INET6_ARPASTRLEN (8*4*2+8+1) // rfc1886
#define NEB_MAX_ARPASTRLEN (INET6_ARPASTRLEN)

/**
 * \param[in] arpa should be at least NEB_*_ARPASTRLEN
 */
extern void neb_netinet_addr_to_arpa(int family, const unsigned char *addr, char *arpa)
	_nattr_nonnull((2, 3));

#endif
