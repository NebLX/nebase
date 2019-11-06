
#include <nebase/netinet.h>

#include <stdio.h>

static neb_net_radix_tree_t rt = NULL;
static int ret = 0;

static void add_subnet(const char *net, int64_t value)
{
	fprintf(stdout, "Adding %s\n", net);

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	if (neb_netinet_net_pton(net, (struct sockaddr *)&addr) != 0) {
		fprintf(stderr, "invalid IPv4 network %s\n", net);
		ret = -1;
		return;
	}

	if (neb_net_radix_tree_set(rt, (struct sockaddr *)&addr, value) != 0) {
		fprintf(stderr, "failed to add %s to radix tree\n", net);
		ret = -1;
	}
}

static void del_subnet(const char *net)
{
	fprintf(stdout, "Deleting %s\n", net);

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	if (neb_netinet_net_pton(net, (struct sockaddr *)&addr) != 0) {
		fprintf(stderr, "invalid IPv4 network %s\n", net);
		ret = -1;
		return;
	}

	neb_net_radix_tree_unset(rt, (struct sockaddr *)&addr);
}

int main(void)
{
	rt = neb_net_radix_tree_create(AF_INET, NULL);
	if (!rt) {
		fprintf(stderr, "failed to create radix tree\n");
		ret = -1;
		goto exit;
	}

	add_subnet("10.0.0.0/8", 1);
	add_subnet("10.10.0.0/24", 2);
	add_subnet("192.168.0.0/16", 3);
	add_subnet("10.10.0.0/24", 4);
	add_subnet("10.10.0.0/25", 5);
	del_subnet("10.10.0.0/25");

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	inet_pton(AF_INET, "10.10.0.23", &addr.sin_addr.s_addr);
	int64_t val = neb_net_radix_tree_lpm_get(rt, (struct sockaddr *)&addr, 0);
	fprintf(stdout, "fetched value: %lld\n", (long long)val);
	if (val != 4) {
		fprintf(stderr, "fetched value mismatch, expect 4\n");
		ret = -1;
	}

exit:
	if (rt)
		neb_net_radix_tree_destroy(rt);
	return ret;
}
