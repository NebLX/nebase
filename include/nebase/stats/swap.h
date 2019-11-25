
#ifndef NEB_STATS_SWAP_H
#define NEB_STATS_SWAP_H 1

#include <nebase/cdefs.h>

#include <sys/types.h>

struct neb_stats_swap;
typedef struct neb_stats_swap* neb_stats_swap_t;

extern neb_stats_swap_t neb_stats_swap_load(void)
	_nattr_warn_unused_result;
extern void neb_stats_swap_release(neb_stats_swap_t s)
	_nattr_nonnull((1));

extern int neb_stats_swap_device_num(const neb_stats_swap_t s)
	_nattr_nonnull((1)) _nattr_pure;

typedef void (* swap_device_each_t)(const char *filename, size_t total, size_t used, void *udata);
extern void neb_stats_swap_device_foreach(const neb_stats_swap_t s, swap_device_each_t f, void *udata)
	_nattr_nonnull((1, 2));

#endif
