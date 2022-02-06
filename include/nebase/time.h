
#ifndef NEB_TIME_H
#define NEB_TIME_H 1

#include "cdefs.h"
#include "sysconf.h"

#include <sys/types.h>
#include <sys/time.h>

/* printf type cast macro */
#define neb_time_sec_ll(x) ((long long)x)
#define neb_time_usec_l(x) ((long)x)
#define neb_time_nsec_l(x) ((long)x)

#define neb_timespecclear(tvp)    ((tvp)->tv_sec = (tvp)->tv_nsec = 0)
#define neb_timespecisset(tvp)    ((tvp)->tv_sec || (tvp)->tv_nsec)
#define neb_timespeccmp(tvp, uvp, cmp)                      \
        (((tvp)->tv_sec == (uvp)->tv_sec) ?                 \
            ((tvp)->tv_nsec cmp (uvp)->tv_nsec) :           \
            ((tvp)->tv_sec cmp (uvp)->tv_sec))

#define neb_timespecadd(tsp, usp, vsp)                      \
    do {                                                    \
        (vsp)->tv_sec = (tsp)->tv_sec + (usp)->tv_sec;      \
        (vsp)->tv_nsec = (tsp)->tv_nsec + (usp)->tv_nsec;   \
        if ((vsp)->tv_nsec >= 1000000000L) {                \
            (vsp)->tv_sec++;                                \
            (vsp)->tv_nsec -= 1000000000L;                  \
        }                                                   \
    } while (0)
#define neb_timespecsub(tsp, usp, vsp)                     \
    do {                                                   \
        (vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec;     \
        (vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec;  \
        if ((vsp)->tv_nsec < 0) {                          \
            (vsp)->tv_sec--;                               \
            (vsp)->tv_nsec += 1000000000L;                 \
        }                                                  \
    } while (0)

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

extern int neb_time_gettimeofday(struct timespec *ts)
	_nattr_warn_unused_result _nattr_nonnull((1));

#endif
