
#include <nebase/syslog.h>
#include <nebase/netinet.h>

#include <stdio.h>
#include <string.h>

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
