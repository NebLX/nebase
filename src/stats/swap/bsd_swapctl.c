
#include <nebase/stats/swap.h>
#include <nebase/syslog.h>

#include <sys/types.h>
#include <sys/swap.h>
#include <sys/stat.h> // for S_BLKSIZE
#include <unistd.h>
#include <stdlib.h>

struct neb_stats_swap {
	struct swapent *entl;
	int count;
};

neb_stats_swap_t neb_stats_swap_load(void)
{
	int nswap = swapctl(SWAP_NSWAP, NULL, 0);
	if (nswap == -1) {
		neb_syslogl(LOG_ERR, "swapctl: %m");
		return NULL;
	}

	struct neb_stats_swap *s = malloc(sizeof(struct neb_stats_swap));
	if (!s) {
		neb_syslogl(LOG_ERR, "malloc: %m");
		return NULL;
	}
	s->count = 0;
	s->entl = NULL;
	if (!nswap) // no swap found
		return s;

	s->entl = malloc(nswap * sizeof(struct swapent));
	if (!s->entl) {
		neb_syslogl(LOG_ERR, "malloc: %m");
		neb_stats_swap_release(s);
		return NULL;
	}

	s->count = swapctl(SWAP_STATS, s->entl, nswap);
	if (s->count == -1) {
		neb_syslogl(LOG_ERR, "swapctl: %m");
		neb_stats_swap_release(s);
		return NULL;
	}

	return s;
}

void neb_stats_swap_release(neb_stats_swap_t s)
{
	if (s->entl)
		free(s->entl);
	free(s);
}

int neb_stats_swap_device_num(const neb_stats_swap_t s)
{
	return s->count;
}

void neb_stats_swap_device_foreach(const neb_stats_swap_t s, swap_device_each_t f, void *udata)
{
	for (int i = 0; i < s->count; i++) {
		struct swapent *ent = s->entl + i;
		f(ent->se_path, ent->se_nblks * S_BLKSIZE, ent->se_inuse * S_BLKSIZE, udata);
	}
}
