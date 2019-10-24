
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/netinet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

const char neb_ipv4_arpa_domain[] = "in-addr.arpa";
const char neb_ipv6_arpa_domain[] = "ip6.arpa";

static void ipv6_addr_to_arpa(const unsigned char addr[16], char *arpa)
{
	static const char hexmap[] = "0123456789abcdef";
	static const int fixed_len = 64;
	memset(arpa, '.', fixed_len);
	for (int i = 0; i < 16; i++) {
		int off = fixed_len - 4 - (i << 2);
		arpa[off] = hexmap[addr[i] & 0x0F];
		arpa[off+2] = hexmap[(addr[i] >> 4) & 0x0F];
	}
	memcpy(arpa+fixed_len, neb_ipv6_arpa_domain, sizeof(neb_ipv6_arpa_domain)-1);
	arpa[NEB_INET6_ARPASTRLEN-1] = '\0';
}

static void ipv4_addr_to_arpa(const unsigned char addr[4], char *arpa)
{
	int len = snprintf(arpa, NEB_INET_ARPASTRLEN, "%u.%u.%u.%u.%s",
	                   addr[3], addr[2], addr[1], addr[0],
	                   neb_ipv4_arpa_domain);
	arpa[len] = '\0';
}

void neb_netinet_addr_to_arpa(int family, const unsigned char *addr, char *arpa)
{
	switch (family) {
	case AF_INET:
		ipv4_addr_to_arpa(addr, arpa);
		break;
	case AF_INET6:
		ipv6_addr_to_arpa(addr, arpa);
		break;
	default:
		arpa[0] = '\0';
		break;
	}
}

int neb_netinet_net_pton(const char *pres, struct sockaddr *netaddr)
{
	switch (netaddr->sa_family) {
	case AF_INET:
	{
		struct sockaddr_in *a4 = (struct sockaddr_in *)netaddr;
#if defined(OS_LINUX) || defined(OSTYPE_BSD) || defined(OS_DARWIN)
		int bits = inet_net_pton(AF_INET, pres, &a4->sin_addr.s_addr, sizeof(struct in_addr));
		if (bits == -1) {
			neb_syslog(LOG_ERR, "Invalid IPv4 network %s: %m", pres);
			return -1;
		}
#elif defined(OSTYPE_SUN)
		int bits;
		if (inet_cidr_pton(AF_INET, pres, &a4->sin_addr.s_addr, &bits) == -1) {
			neb_syslog(LOG_ERR, "Invalid IPv4 network %s: %m", pres);
			return -1;
		}
		if (bits == -1)
			bits = sizeof(struct in_addr) << 3;
#else
# error "fix me"
#endif
		a4->sin_port = bits;
	}
		break;
	case AF_INET6:
	{
		struct sockaddr_in6 *a6 = (struct sockaddr_in6 *)netaddr;
#if defined(OS_OPENBSD) || defined(OS_DFLYBSD) || defined(OS_DARWIN)
		int bits = inet_net_pton(AF_INET6, pres, &a6->sin6_addr.s6_addr, sizeof(struct in6_addr));
		if (bits == -1) {
			neb_syslog(LOG_ERR, "Invalid IPv6 network %s: %m", pres);
			return -1;
		}
#elif defined(OSTYPE_SUN)
		int bits;
		if (inet_cidr_pton(AF_INET6, pres, &a6->sin6_addr.s6_addr, &bits) == -1) {
			neb_syslog(LOG_ERR, "Invalid IPv6 network %s: %m", pres);
			return -1;
		}
		if (bits == -1)
			bits = sizeof(struct in6_addr) << 3;
#elif defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_NETBSD)
		int bits = sizeof(struct in6_addr) << 3;
		const char *d = strchr(pres, '/');
		if (d) {
			int len = d - pres;
			if (len >= INET6_ADDRSTRLEN) {
				neb_syslog(LOG_ERR, "Invalid IPv6 network %s: invalid address string length", pres);
				return -1;
			}

			char *endptr;
			long prefix = strtol(d+1, &endptr, 10);
			if (endptr && *endptr) {
				neb_syslog(LOG_ERR, "Invalid IPv6 network %s: invalid prefix length", pres);
				return -1;
			}
			if (prefix < 0 || prefix > bits) {
				neb_syslog(LOG_ERR, "Invalid IPv6 network %s: invalid prefix length", pres);
				return -1;
			}
			bits = prefix;

			char buf[INET6_ADDRSTRLEN];
			memcpy(buf, pres, len);
			buf[len] = 0;
			if (inet_pton(AF_INET6, buf, &a6->sin6_addr.s6_addr) != 1) {
				neb_syslog(LOG_ERR, "Invalid IPv6 network %s: invalid address", pres);
				return -1;
			}
		} else {
			if (inet_pton(AF_INET6, pres, &a6->sin6_addr.s6_addr) != 1) {
				neb_syslog(LOG_ERR, "Invalid IPv6 network %s: invalid address", pres);
				return -1;
			}
		}
#else
# error "fix me"
#endif
		a6->sin6_port = bits;
	}
		break;
	default:
		neb_syslog(LOG_ERR, "net_pton: unsupported family %u", netaddr->sa_family);
		return -1;
		break;
	}
	return 0;
}

void neb_netinet_fill_mask(unsigned char *addr, int prefix)
{
	if (prefix > 8) {
		*addr = 0xFF;
		neb_netinet_fill_mask(addr + 1, prefix-8);
	} else {
		*addr = ((0xFF << (8 - prefix)) & 0xFF);
		return;
	}
}
