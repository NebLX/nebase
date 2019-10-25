
#include <nebase/netinet.h>

#include <stdio.h>

static int ret = 0;

static void do_test(const char *netaddr, int prefix)
{
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	fprintf(stdout, "Testing %s: ", netaddr);
	if (neb_netinet_net_pton(netaddr, (struct sockaddr *)&addr) != 0 || addr.sin_port != prefix) {
		fprintf(stderr, "FAIL\n");
		ret = -1;
	} else {
		fprintf(stdout, "OK\n");
	}
}

int main(void)
{
	do_test("10.0.0.0/8", 8);
	do_test("10.1.0.0/16", 16);
	do_test("10.1.0.1/32", 32);
	do_test("10.1.0.1", 32);

	return ret;
}
