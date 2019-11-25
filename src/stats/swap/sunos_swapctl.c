
#include <nebase/stats/swap.h>
#include <nebase/syslog.h>
#include <nebase/obstack.h>

#include <sys/stat.h>
#include <sys/swap.h>
#include <stdlib.h>

#define SWAP_PATH_MAXSTRSIZE 80 // see swapctl(2)

struct neb_stats_swap {
	struct obstack obs;
	struct swaptable *table;
	int count;
};

neb_stats_swap_t neb_stats_swap_load(void)
{
	int nswap = swapctl(SC_GETNSWP, NULL);
	if (nswap == -1) {
		neb_syslogl(LOG_ERR, "swapctl: %m");
		return NULL;
	}

	struct neb_stats_swap *s = malloc(sizeof(struct neb_stats_swap));
	if (!s) {
		neb_syslogl(LOG_ERR, "malloc: %m");
		return NULL;
	}
	obstack_init(&s->obs);
	s->count = 0;
	s->table = NULL;
	if (!nswap) // no swap found
		return s;

	s->table = obstack_alloc(&s->obs, sizeof(struct swaptable) + nswap * (sizeof(struct swapent)));
	if (!s->table) {
		neb_syslogl(LOG_ERR, "obstack_alloc: %m");
		neb_stats_swap_release(s);
		return NULL;
	}
	s->table->swt_n = nswap;
	for (int i = 0; i < nswap; i++) {
		struct swapent *ent = s->swt_ent + i;
		ent->ste_path = obstack_alloc(&s->obs, SWAP_PATH_MAXSTRSIZE);
		if (!ent->ste_path) {
			neb_syslogl(LOG_ERR, "obstack_alloc: %m");
			neb_stats_swap_release(s);
			return NULL;
		}
	}

	s->count = swapctl(SC_LIST, s);
	if (s->count == -1) {
		neb_syslogl(LOG_ERR, "swapctl: %m");
		neb_stats_swap_release(s);
		return NULL;
	}

	return s;
}

void neb_stats_swap_release(neb_stats_swap_t s)
{
	obstack_free(&s->obs, NULL);
	free(s);
}

int neb_stats_swap_device_num(const neb_stats_swap_t s)
{
	return s->count;
}

void neb_stats_swap_device_foreach(const neb_stats_swap_t s, swap_device_each_t f, void *udata)
{
	for (int i = 0; i < s->count; i++) {
		struct swapent *ent = s->swt_ent + i; // TODO pagesize
		f(ent->ste_path, ent->ste_pages, ent->ste_pages - ent->ste_free, udata);
	}
}
