
#ifndef NEB_NET_RADIX_H
#define NEB_NET_RADIX_H 1

#include <nebase/cdefs.h>

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

#endif
