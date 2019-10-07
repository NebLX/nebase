
#ifndef NEB_RANDOM_H
#define NEB_RANDOM_H 1

#include "cdefs.h"

#include <sys/types.h>
#include <stdint.h>

extern uint32_t neb_random_uint32(void);
extern void neb_random_buf(void *buf, size_t nbytes);
/**
 * \param[in] upper_bound should be more than 2
 * \return a single 32-bit value, uniformly distributed but less than upper_bound
 */
extern uint32_t neb_random_uniform(uint32_t upper_bound);

#endif
