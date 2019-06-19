
#ifndef NEB_COMPAT_SYS_TIME_H
#define NEB_COMPAT_SYS_TIME_H 1

#include_next <sys/time.h>

#ifndef timespecsub
# define timespecsub(tsp, usp, vsp)                                     \
        do {                                                            \
                (vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec;          \
                (vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec;       \
                if ((vsp)->tv_nsec < 0) {                               \
                        (vsp)->tv_sec--;                                \
                        (vsp)->tv_nsec += 1000000000L;                  \
                }                                                       \
        } while (0)
#endif

#endif
