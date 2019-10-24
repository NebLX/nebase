
#include <nebase/syslog.h>
#include <nebase/netinet.h>

#include <net/radix.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct neb_net_radix_tree {
	struct radix_node_head *rnh;

	sa_family_t family;
	neb_net_rte_del_cb on_del;
};

struct neb_net_radix_entry {
	struct radix_node nodes[2];

	struct neb_net_radix_tree *ref_rt;
	struct sockaddr *network;
	struct sockaddr *netmask;

	int64_t udata;
};

// Make sure we can use sockaddr as key
_Static_assert(offsetof(struct sockaddr_in, sin_port) > 1,
	"sin_port should not at begining of sockaddr_in");
_Static_assert(offsetof(struct sockaddr_in, sin_addr) > offsetof(struct sockaddr_in, sin_port),
	"sockaddr_in: sin_port should be before sin_addr");
_Static_assert(offsetof(struct sockaddr_in6, sin6_port) > 1,
	"sin6_port should not at begining of sockaddr_in6");
_Static_assert(offsetof(struct sockaddr_in6, sin6_addr) > offsetof(struct sockaddr_in6, sin6_port),
	"sockaddr_in6: sin6_port should be before sin6_addr");

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
static struct sockaddr *rt_get_network(struct sockaddr *netaddr)
{
	void *vk = NULL;
	switch (netaddr->sa_family) {
	case AF_INET:
	{
		struct sockaddr_in *a4 = (struct sockaddr_in *)netaddr;
		in_port_t prefix = a4->sin_port;
		if (prefix >= (sizeof(struct in_addr) << 3)) {
			neb_syslog(LOG_ERR, "IPv4 network prefix should be less than %d", (int)(sizeof(struct in_addr) << 3));
			return NULL;
		}
		vk = malloc(sizeof(struct sockaddr_in));
		if (!vk) {
			neb_syslog(LOG_ERR, "malloc: %m");
			return NULL;
		}
		memcpy(vk, netaddr, sizeof(struct sockaddr_in));
		*((uint8_t *)vk) = offsetof(struct sockaddr_in, sin_addr) + sizeof(struct in_addr);
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
		vk = malloc(sizeof(struct sockaddr_in6));
		if (!vk) {
			neb_syslog(LOG_ERR, "malloc: %m");
			return NULL;
		}
		memcpy(vk, netaddr, sizeof(struct sockaddr_in6));
		*((uint8_t *)vk) = offsetof(struct sockaddr_in6, sin6_addr) + sizeof(struct in6_addr);
	}
		break;
	default:
		neb_syslog(LOG_ERR, "Unsupported network address family: %u", netaddr->sa_family);
		break;
	}
	return vk;
}

static struct sockaddr *rt_get_netmask(int family, struct sockaddr *vk)
{
	void *mk = NULL;
	switch (family) {
	case AF_INET:
	{
		mk = calloc(1, sizeof(struct sockaddr_in));
		if (!mk) {
			neb_syslog(LOG_ERR, "malloc: %m");
			return NULL;
		}
		memcpy(mk, vk, offsetof(struct sockaddr_in, sin_addr));
		struct sockaddr_in *a4 = (struct sockaddr_in *)mk;
		neb_netinet_fill_mask((unsigned char *)&a4->sin_addr.s_addr, a4->sin_port);
	}
		break;
	case AF_INET6:
	{
		mk = calloc(1, sizeof(struct sockaddr_in6));
		if (!mk) {
			neb_syslog(LOG_ERR, "malloc: %m");
			return NULL;
		}
		memcpy(mk, vk, offsetof(struct sockaddr_in6, sin6_addr));
		struct sockaddr_in6 *a6 = (struct sockaddr_in6 *)mk;
		neb_netinet_fill_mask((unsigned char *)&a6->sin6_addr.s6_addr, a6->sin6_port);
	}
		break;
	default:
		neb_syslog(LOG_ERR, "Unsupported netmask address family: %u", family);
		break;
	}
	return mk;
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

	rte->netmask = rt_get_netmask(netaddr->sa_family, rte->network);
	if (!rte->netmask) {
		neb_net_radix_entry_del(rte);
		return NULL;
	}

	return rte;
}

neb_net_radix_tree_t neb_net_radix_tree_create(int family , neb_net_rte_del_cb on_del)
{
	int offset = 0;
	switch (family) {
	case AF_INET:
		offset = offsetof(struct sockaddr_in, sin_addr) << 3;
		break;
	case AF_INET6:
		offset = offsetof(struct sockaddr_in6, sin6_addr) << 3;
		break;
	default:
		neb_syslog(LOG_ERR, "radix tree: unsupported family %d", family);
		return NULL;
		break;
	}

	struct neb_net_radix_tree *rt = calloc(1, sizeof(struct neb_net_radix_tree));
	if (!rt) {
		neb_syslog(LOG_ERR, "calloc: %m");
		return NULL;
	}

	if (!rn_inithead((void **)&rt->rnh, offset)) {
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

int neb_net_radix_tree_set(neb_net_radix_tree_t rt, struct sockaddr *netaddr, int64_t data)
{
	if (rt->family != netaddr->sa_family) {
		neb_syslog(LOG_ERR, "radix tree family mismatch");
		return -1;
	}
	if (data == 0) {
		neb_syslog(LOG_ERR, "radix tree entry associated data should not be zero");
		return -1;
	}

	struct neb_net_radix_entry *rte = neb_net_radix_entry_new(netaddr);
	if (!rte)
		return -1;

	struct radix_node *x = rn_addroute(rte->network, rte->netmask, &rt->rnh->rh, rte->nodes);
	if (!x) { // existed
		x = rn_lookup(rte->network, rte->netmask, &rt->rnh->rh);
		neb_net_radix_entry_del(rte);
		if (!x) {
			neb_syslog(LOG_ERR, "failed to add new entry to radix tree");
			return -1;
		}
		rte = (struct neb_net_radix_entry *)x;
		if (rt->on_del) {
			rt->on_del(rte->udata);
			rte->udata = 0;
		}
	}

	rte->udata = data;
	rte->ref_rt = rt;
	return 0;
}

int64_t neb_net_radix_tree_lpm_get(neb_net_radix_tree_t rt, struct sockaddr *ipaddr, bool fill)
{
	if (rt->family != ipaddr->sa_family)
		return 0;
	int len = 0;
	switch (ipaddr->sa_family) {
	case AF_INET:
		len = offsetof(struct sockaddr_in, sin_port) + sizeof(struct in_addr);
		break;
	case AF_INET6:
		len = offsetof(struct sockaddr_in6, sin6_port) + sizeof(struct in6_addr);
		break;
	default:
		return 0;
		break;
	}
	uint8_t c = *(uint8_t *)ipaddr;
	*(uint8_t *)ipaddr = len;
	struct radix_node *x = rn_match(ipaddr, &rt->rnh->rh);
	*(uint8_t *)ipaddr = c;
	if (!x)
		return 0;
	struct neb_net_radix_entry *rte = (struct neb_net_radix_entry *)x;
	if (fill) {
		switch (rt->family) {
		case AF_INET:
			((struct sockaddr_in *)ipaddr)->sin_port = ((struct sockaddr_in *)rte->network)->sin_port;
			((struct sockaddr_in *)ipaddr)->sin_addr.s_addr = ((struct sockaddr_in *)rte->network)->sin_addr.s_addr;
			break;
		case AF_INET6:
			((struct sockaddr_in6 *)ipaddr)->sin6_port = ((struct sockaddr_in6 *)rte->network)->sin6_port;
			memcpy(&((struct sockaddr_in6 *)ipaddr)->sin6_addr.s6_addr, ((struct sockaddr_in6 *)rte->network)->sin6_addr.s6_addr, sizeof(struct in6_addr));
			break;
		default:
			break;
		}
	}
	return ((struct neb_net_radix_entry *)x)->udata;
}
