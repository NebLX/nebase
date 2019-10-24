
#ifndef NEB_NETINET_H
#define NEB_NETINET_H 1

#include "cdefs.h"

/*
 * Radix trees
 */

#include <stdint.h>
#include <stdbool.h>

struct sockaddr;

typedef struct neb_net_radix_tree* neb_net_radix_tree_t;
typedef void (* neb_net_rte_del_cb)(int64_t data);

extern neb_net_radix_tree_t neb_net_radix_tree_create(int family, neb_net_rte_del_cb on_del)
	_nattr_warn_unused_result;
extern void neb_net_radix_tree_destroy(neb_net_radix_tree_t rt)
	_nattr_nonnull((1));

/**
 * \brief add or reset network (should not be exact address) entry
 * \param[in] netaddr the port field should be the prefix length
 *                    the family field should match the family of the tree
 * \param[in] data should not be zero
 * \note if on_del is set on rt, the old existed data will send to on_del
 */
extern int neb_net_radix_tree_set(neb_net_radix_tree_t rt, struct sockaddr *netaddr, int64_t data)
	_nattr_warn_unused_result _nattr_nonnull((1));
/**
 * \param[in] netaddr the port field should be the prefix length
 *                    the family field should match the family of the tree
 */
extern void neb_net_radix_tree_unset(neb_net_radix_tree_t rt, struct sockaddr *netaddr)
	_nattr_nonnull((1, 2));

/**
 * \param[in,out] ipaddr family and addr should be set as input
 * \param[in] fill if set, port will be set to prefix length,
 *                 and addr will be set to the matched network address
 */
extern int64_t neb_net_radix_tree_lpm_get(neb_net_radix_tree_t rt, struct sockaddr *ipaddr, bool fill)
	_nattr_warn_unused_result _nattr_nonnull((1, 2));

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
