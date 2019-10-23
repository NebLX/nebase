
#ifndef NEB_NETINET_H
#define NEB_NETINET_H 1

#include "cdefs.h"

/*
 * Radix trees
 */

#include <stdint.h>

struct sockaddr;

typedef struct neb_net_radix_tree* neb_net_radix_tree_t;
typedef void (* neb_net_rte_del_cb)(int64_t data);

extern neb_net_radix_tree_t neb_net_radix_tree_create(int family, neb_net_rte_del_cb on_del)
	_nattr_warn_unused_result;
extern void neb_net_radix_tree_destroy(neb_net_radix_tree_t rt)
	_nattr_nonnull((1));

/**
 * \param[in] netaddr the port field should be the prefix length
 *                    the family field should match the family of the tree
 */
extern int neb_net_radix_tree_add(neb_net_radix_tree_t rt, struct sockaddr *netaddr, int64_t data)
	_nattr_warn_unused_result _nattr_nonnull((1));

/*
 * Util functions
 */

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
 * \param[in] addr should be all zero, and have enough space than prefix/8 + 1
 */
extern void neb_netinet_fill_mask(unsigned char *addr, int prefix);

#endif
