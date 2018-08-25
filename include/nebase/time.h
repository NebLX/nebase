
#ifndef NEB_TIME_H
#define NEB_TIME_H 1

#include <sys/types.h>

/**
 * \return 0 if failed, or the real time
 */
extern time_t neb_time_up(void);

/**
 * \return 0 if failed, or the real time
 */
extern time_t neb_time_boot(void);

#endif
