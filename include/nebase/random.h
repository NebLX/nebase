
#ifndef NEB_RANDOM_H
#define NEB_RANDOM_H 1

#include "cdefs.h"

#include <sys/types.h>
#include <stdint.h>

/*
 * Basic random functions
 */

#define NEB_RANDOM_MIN_UPPER_BOUND 2

extern uint32_t neb_random_uint32(void);
extern void neb_random_buf(void *buf, size_t nbytes);
/**
 * \param[in] upper_bound should not be less then NEB_RANDOM_MIN_UPPER_BOUND
 * \return a single 32-bit value, uniformly distributed but less than upper_bound
 */
extern uint32_t neb_random_uniform(uint32_t upper_bound);

/*
 * Random pool
 */

typedef struct neb_random_pool *neb_random_pool_t;
typedef struct neb_random_pool_node *neb_random_pool_node_t;

extern neb_random_pool_t neb_random_pool_create(void)
	_nattr_warn_unused_result;
extern void neb_random_pool_destroy(neb_random_pool_t p)
	_nattr_nonnull((1));
extern int neb_random_pool_add(neb_random_pool_t p, int64_t value)
	_nattr_warn_unused_result _nattr_nonnull((1));
extern int neb_random_pool_add_range(neb_random_pool_t p, int min, int max, int step)
	_nattr_warn_unused_result _nattr_nonnull((1));
extern void neb_random_pool_forge(neb_random_pool_t p)
	_nattr_nonnull((1));

extern neb_random_pool_node_t neb_random_pool_pick(neb_random_pool_t p)
	_nattr_warn_unused_result _nattr_nonnull((1));
extern void neb_random_pool_put(neb_random_pool_t p, neb_random_pool_node_t n)
	_nattr_nonnull((1, 2));

extern int64_t neb_random_pool_node_value(neb_random_pool_node_t n)
	_nattr_nonnull((1));

#endif
