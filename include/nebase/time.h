
#ifndef NEB_TIME_H
#define NEB_TIME_H 1

#include "cdefs.h"
#include "sysconf.h"

#include <sys/types.h>
#include <sys/time.h>

#define neb_timespecsub3(tsp, usp, vsp)                    \
    do {                                                   \
        (vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec;     \
        (vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec;  \
        if ((vsp)->tv_nsec < 0) {                          \
            (vsp)->tv_sec--;                               \
            (vsp)->tv_nsec += 1000000000L;                 \
        }                                                  \
    } while (0)
#define neb_timespecsub2(vvp, uvp) neb_timespecsub3(vvp, uvp, vvp)

static inline void neb_clktck2timeval(unsigned long c, struct timeval *tv)
{
	tv->tv_sec = c / neb_sysconf_clock_ticks;
	tv->tv_usec = (c % neb_sysconf_clock_ticks) * 1000000 / neb_sysconf_clock_ticks;
}

/**
 * \return 0 if failed, or the real time
 * \note may be not thread safe
 */
extern time_t neb_time_up(void);

/**
 * \return 0 if failed, or the real time
 * \note may be not thread safe
 */
extern time_t neb_time_boot(void);

extern int neb_daytime_abs_nearest(int sec_of_day, time_t *abs_ts, int *delta_sec)
	_nattr_nonnull((2, 3));

extern int neb_time_gettime_fast(struct timespec *ts)
	_nattr_warn_unused_result _nattr_nonnull((1));
extern int64_t neb_time_get_msec(void);

extern int neb_time_gettimeofday(struct timespec *ts)
	_nattr_warn_unused_result _nattr_nonnull((1));

#endif
