
#include <nebase/stats/swap.h>
#include <nebase/syslog.h>

#include <sys/sysctl.h>
#include <stdlib.h>

struct neb_stats_swap {
	struct xsw_usage xsu;
};

neb_stats_swap_t neb_stats_swap_load(void)
{
	struct neb_stats_swap *s = calloc(1, sizeof(struct xsw_usage));
	if (!s) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	int mib[2] = { CTL_VM, VM_SWAPUSAGE };
	int miblen = 2;
	size_t len = sizeof(s->xsu);
	if (sysctl(mib, miblen, &s->xsu, &len, NULL, 0) == -1) {
		neb_syslogl(LOG_ERR, "sysctl(CTL_VM/VM_SWAPUSAGE): %m");
		neb_stats_swap_release(s);
		return NULL;
	}

	return s;
}

void neb_stats_swap_release(neb_stats_swap_t s)
{
	free(s);
}

int neb_stats_swap_device_num(const neb_stats_swap_t s)
{
	return s->xsu.xsu_total == 0 ? 0 : 1;
}

void neb_stats_swap_device_foreach(const neb_stats_swap_t s, swap_device_each_t f, void *udata)
{
	if (s->xsu.xsu_total)
		f("private", s->xsu.xsu_total * s->xsu.xsu_pagesize, s->xsu.xsu_used * s->xsu.xsu_pagesize, udata);
}
