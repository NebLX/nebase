
#include <nebase/net/ipaddr.h>

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s <0-32>\n", argv[0]);
		return -1;
	}
	int prefix = atoi(argv[1]);
	if (prefix < 0 || prefix > 32) {
		fprintf(stderr, "prefix %s is out of range\n", argv[1]);
		return -1;
	}
	fprintf(stdout, "prefix: %d\n", prefix);

	struct in_addr addr = NEB_STRUCT_INITIALIZER;
	neb_netinet_fill_mask((unsigned char *)&addr, prefix);

	char buf[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &addr, buf, INET_ADDRSTRLEN);
	fprintf(stdout, "netmask: %s\n", buf);

	return 0;
}
