
#ifndef NEB_TIME_H
#define NEB_TIME_H 1

#include "cdefs.h"

#include <sys/types.h>

#include <time.h>

/**
 * \return 0 if failed, or the real time
 */
extern time_t neb_time_up(void);

/**
 * \return 0 if failed, or the real time
 */
extern time_t neb_time_boot(void);

extern int neb_daytime_abs_nearest(int sec_of_day, time_t *abs_ts, int *delta_sec)
	neb_attr_nonnull((2, 3));

extern int64_t neb_time_get_msec(void);

#endif
