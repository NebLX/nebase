
#include <nebase/stats/swap.h>
#include <nebase/syslog.h>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <kvm.h>

struct neb_stats_swap {
	struct kvm_swap *ksp;
	int count;
};

static int get_nswap(void)
{
	int name[2];
	size_t name_count = 2;

	sysctlnametomib("vm.nswapdev", name, &name_count);
	int nswap = 0;
	size_t len = sizeof(nswap);
	int ret = sysctl(name, name_count, &nswap, &len, (void *)0, 0);
	if (ret == -1) {
		neb_syslogl(LOG_ERR, "sysctl: %m");
		return -1;
	}
	return nswap;
}

neb_stats_swap_t neb_stats_swap_load(void)
{
	int nswap = get_nswap();
	if (nswap == -1)
		return NULL;

	struct neb_stats_swap *s = malloc(sizeof(struct neb_stats_swap));
	if (!s) {
		neb_syslogl(LOG_ERR, "malloc: %m");
		return NULL;
	}
	s->count = 0;
	s->ksp = NULL;
	if (!nswap) // no swap found
		return s;

	char errbuf[LINE_MAX];
	kvm_t *kh = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf);
	if (!kh) {
		neb_syslogl(LOG_ERR, "kvm_openfiles: %s", errbuf);
		neb_stats_swap_release(s);
		return NULL;
	}

	int maxswap = nswap + 1; // NOTE including the total one
	s->ksp = malloc(sizeof(struct kvm_swap) * maxswap);
	if (!s->ksp) {
		neb_syslogl(LOG_ERR, "malloc: %m");
		kvm_close(kh);
		neb_stats_swap_release(s);
		return NULL;
	}

	s->count = kvm_getswapinfo(kh, s->ksp, maxswap, 0);
	if (s->count == -1) {
		neb_syslogl(LOG_ERR, "kvm_getswapinfo: %m");
		kvm_close(kh);
		neb_stats_swap_release(s);
		return NULL;
	}

	kvm_close(kh);
	return s;
}

void neb_stats_swap_release(neb_stats_swap_t s)
{
	if (s->ksp)
		free(s->ksp);
	free(s);
}

int neb_stats_swap_device_num(const neb_stats_swap_t s)
{
	return s->count;
}

void neb_stats_swap_device_foreach(const neb_stats_swap_t s, swap_device_each_t f, void *udata)
{
	for (int i = 0; i < s->count; i++) {
		struct kvm_swap *ksw = s->ksp + i;
		f(ksw->ksw_devname, ksw->ksw_total, ksw->ksw_used, udata);
	}
}
