
#include "options.h"

#include <nebase/random.h>

#if defined(OS_LINUX)
# include <bsd/stdlib.h>
#else
# include <stdlib.h>
#endif

uint32_t neb_random_uint32(void)
{
	return arc4random();
}

void neb_random_buf(void *buf, size_t nbytes)
{
	arc4random_buf(buf, nbytes);
}

uint32_t neb_random_uniform(uint32_t upper_bound)
{
	return arc4random_uniform(upper_bound);
}
