
#include <nebase/net/ipaddr.h>

#include <stdio.h>

static int ret = 0;

static void do_test(const char *netaddr, int prefix)
{
	struct sockaddr_in6 addr;
	addr.sin6_family = AF_INET6;
	fprintf(stdout, "Testing %s: ", netaddr);
	if (neb_netinet_net_pton(netaddr, (struct sockaddr *)&addr) != 0 || addr.sin6_port != prefix) {
		fprintf(stderr, "FAIL\n");
		ret = -1;
	} else {
		fprintf(stdout, "OK\n");
	}
}

int main(void)
{
	do_test("2001::/16", 16);
	do_test("2001:1::/32", 32);
	do_test("2001:1:2::/48", 48);
	do_test("2001:1:2:3::/64", 64);
	do_test("2001:1::1/128", 128);
	do_test("2001:1::1", 128);

	return ret;
}
