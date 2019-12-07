
#ifndef NEB_NET_IPADDR_H
#define NEB_NET_IPADDR_H 1

#include <nebase/cdefs.h>

#include <arpa/inet.h>

#define NEB_INET_ARPASTRLEN (INET_ADDRSTRLEN+1+12) // rfc1035
#define NEB_INET6_ARPASTRLEN (8*4*2+8+1) // rfc1886
#define NEB_MAX_ARPASTRLEN (INET6_ARPASTRLEN)

/**
 * \param[in] arpa should be at least NEB_*_ARPASTRLEN
 */
extern void neb_netinet_addr_to_arpa(int family, const unsigned char *addr, char *arpa)
	_nattr_nonnull((2, 3));

/**
 * \brief get the next ip address
 * \note no bounding check here
 */
extern void neb_netinet_addr_next(struct sockaddr *ipaddr)
	_nattr_nonnull((1));

/**
 * \param[in] pres the network string
 * \param[in,out] netaddr the family field should be set, and the space should be enough
 *                        the port field will be set to the prefix length
 */
extern int neb_netinet_net_pton(const char *pres, struct sockaddr *netaddr)
	_nattr_nonnull((1, 2));

/**
 * \param[in] addr should be all zero, and have enough space than prefix/8 + 1
 */
extern void neb_netinet_fill_mask(unsigned char *addr, int prefix);

#endif
