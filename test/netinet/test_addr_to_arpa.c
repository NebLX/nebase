
#include <nebase/netinet.h>

#include <stdio.h>
#include <string.h>
#include <netinet/in.h>

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s <ip address>\n", argv[0]);
		return -1;
	}
	const char *addr = argv[1];
	if (strchr(addr, ':')) {
		struct in6_addr v6a;
		if (inet_pton(AF_INET6, addr, &v6a) != 1) {
			perror("inet_pton");
			return -1;
		}

		char arpa[NEB_INET6_ARPASTRLEN];
		neb_netinet_addr_to_arpa(AF_INET6, (const unsigned char *)&v6a, arpa);
		fprintf(stdout, "%s\n", arpa);
	} else {
		struct in_addr v4a;
		if (inet_pton(AF_INET, addr, &v4a) != 1) {
			perror("inet_pton");
			return -1;
		}

		char arpa[NEB_INET_ARPASTRLEN];
		neb_netinet_addr_to_arpa(AF_INET, (const unsigned char *)&v4a, arpa);
		fprintf(stdout, "%s\n", arpa);
	}
	return 0;
}
