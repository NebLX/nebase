
#include <nebase/stats/swap.h>

#include "main.h"

#include <stdio.h>

static void print_swap(const char *filename, size_t total, size_t used, void *udata _nattr_unused)
{
	fprintf(stdout, "%s: total %zu, used %zu\n", filename, total, used);
}

void print_stats_swap(void)
{
	neb_stats_swap_t s = neb_stats_swap_load();
	if (!s) {
		fprintf(stderr, "failed to get swap stats\n");
		return;
	}

	neb_stats_swap_device_foreach(s, print_swap, NULL);

	neb_stats_swap_release(s);
}
