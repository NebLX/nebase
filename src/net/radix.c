
#include <nebase/syslog.h>
#include <nebase/netinet.h>

#include <net/radix.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct neb_netaddr {
	uint8_t len;
	uint8_t prefix;
	uint16_t family;
	unsigned char addr[];
} _nattr_packed;

struct neb_net_radix_tree {
	struct radix_node_head *rnh;

	sa_family_t family;
	neb_net_rte_del_cb on_del;
};

struct neb_net_radix_entry {
	struct radix_node nodes[2];

	struct neb_net_radix_tree *ref_rt;
	struct neb_netaddr *network;
	struct neb_netaddr *netmask;

	int64_t udata;
};

static void neb_net_radix_entry_del(struct neb_net_radix_entry *rte)
{
	if (rte->ref_rt && rte->ref_rt->on_del) {
		rte->ref_rt->on_del(rte->udata);
		rte->udata = 0;
	}
	if (rte->network) {
		free(rte->network);
		rte->network = NULL;
	}
	if (rte->netmask) {
		free(rte->netmask);
		rte->netmask = NULL;
	}
	free(rte);
}

/**
 * \param[in] netaddr the port field should be the prefix length
 */
static struct neb_netaddr *rt_get_network(struct sockaddr *netaddr)
{
	struct neb_netaddr *network = NULL;
	switch (netaddr->sa_family) {
	case AF_INET:
	{
		struct sockaddr_in *a4 = (struct sockaddr_in *)netaddr;
		in_port_t prefix = a4->sin_port;
		if (prefix >= (sizeof(struct in_addr) << 3)) {
			neb_syslog(LOG_ERR, "IPv4 network prefix should be less than %d", (int)(sizeof(struct in_addr) << 3));
			return NULL;
		}
		uint8_t len = sizeof(struct neb_netaddr) + sizeof(struct in_addr);
		network = malloc(len);
		if (!network) {
			neb_syslog(LOG_ERR, "malloc: %m");
			return NULL;
		}
		network->len = len;
		network->family = AF_INET;
		network->prefix = prefix;
		memcpy(network->addr, &a4->sin_addr.s_addr, sizeof(struct in_addr));
	}
		break;
	case AF_INET6:
	{
		struct sockaddr_in6 *a6 = (struct sockaddr_in6 *)netaddr;
		in_port_t prefix = a6->sin6_port;
		if (prefix >= (sizeof(struct in6_addr) << 3)) {
			neb_syslog(LOG_ERR, "IPv6 network prefix should be less than %d", (int)(sizeof(struct in6_addr) << 3));
			return NULL;
		}
		uint8_t len = sizeof(struct neb_netaddr) + sizeof(struct in6_addr);
		network = malloc(len);
		if (!network) {
			neb_syslog(LOG_ERR, "malloc: %m");
			return NULL;
		}
		network->len = len;
		network->family = AF_INET6;
		network->prefix = prefix;
		memcpy(network->addr, &a6->sin6_addr.s6_addr, sizeof(struct in6_addr));
	}
		break;
	default:
		neb_syslog(LOG_ERR, "Unsupported network address family: %u", netaddr->sa_family);
		break;
	}
	return network;
}

static struct neb_netaddr *rt_get_netmask(struct neb_netaddr *network)
{
	struct neb_netaddr *netmask = calloc(1, network->len);
	memcpy(netmask, network, sizeof(struct neb_netaddr));
	neb_netinet_fill_mask(netmask->addr, netmask->prefix);
	return netmask;
}

/**
 * \param[in] netaddr the port field should be the prefix length
 */
static struct neb_net_radix_entry *neb_net_radix_entry_new(struct sockaddr *netaddr)
{
	struct neb_net_radix_entry *rte = calloc(1, sizeof(struct neb_net_radix_entry));
	if (!rte) {
		neb_syslog(LOG_ERR, "calloc: %m");
		return NULL;
	}

	rte->network = rt_get_network(netaddr);
	if (!rte->network) {
		neb_net_radix_entry_del(rte);
		return NULL;
	}

	rte->netmask = rt_get_netmask(rte->network);
	if (!rte->netmask) {
		neb_net_radix_entry_del(rte);
		return NULL;
	}

	return rte;
}

neb_net_radix_tree_t neb_net_radix_tree_create(int family , neb_net_rte_del_cb on_del)
{
	AF_INET;
	struct neb_net_radix_tree *rt = calloc(1, sizeof(struct neb_net_radix_tree));
	if (!rt) {
		neb_syslog(LOG_ERR, "calloc: %m");
		return NULL;
	}

	if (!rn_inithead((void **)&rt->rnh, offsetof(struct neb_netaddr, addr) << 3)) {
		neb_syslog(LOG_ERR, "rn_inithead failed");
		free(rt);
		return NULL;
	}

	rt->family = family;
	rt->on_del = on_del;
	return rt;
}

static int neb_rt_freeentry(struct radix_node *rn, void *arg)
{
	struct radix_head * const rnh = arg;
	struct neb_net_radix_entry *rte = (struct neb_net_radix_entry *)rn;

	struct radix_node *x = rn_delete(rte->network, rte->netmask, rnh);
	if (x != NULL)
		neb_net_radix_entry_del((struct neb_net_radix_entry *)x);
	return 0;
}

void neb_net_radix_tree_destroy(neb_net_radix_tree_t rt)
{
	rn_walktree(&rt->rnh->rh, neb_rt_freeentry, rt->rnh);
	if (rt->rnh)
		rn_detachhead((void **)&rt->rnh);
	free(rt);
}

int neb_net_radix_tree_add(neb_net_radix_tree_t rt, struct sockaddr *netaddr, int64_t data)
{
	if (rt->family != netaddr->sa_family) {
		neb_syslog(LOG_ERR, "family mismatch");
		return -1;
	}
	struct neb_net_radix_entry *rte = neb_net_radix_entry_new(netaddr);
	if (!rte)
		return -1;

	struct radix_node *x = rn_addroute(rte->network, rte->netmask, &rt->rnh->rh, rte->nodes);
	if (!x) {
		// FIXME seems to be existed ?
		neb_syslog(LOG_ERR, "failed to add to radix tree");
		neb_net_radix_entry_del(rte);
		return -1;
	}
	if ((void *)x != (void *)rte) {
		neb_syslog(LOG_INFO, "x(%p) != rte(%p)", x, rte); // FIXME really needed?
		neb_net_radix_entry_del(rte);
		rte = (struct neb_net_radix_entry *)x;
	}

	rte->udata = data;
	rte->ref_rt = rt;
	return 0;
}
