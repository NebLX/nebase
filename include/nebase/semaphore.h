
#ifndef NEB_SEMAPHORE_H
#define NEB_SEMAPHORE_H 1

#include "cdefs.h"

#include <time.h>

typedef void * neb_sem_t;

/*
 * Notify Semaphore
 *   For waiting for a signal
 */

extern neb_sem_t neb_sem_notify_create(unsigned int value)
	__attribute_warn_unused_result__;
extern void neb_sem_notify_destroy(neb_sem_t sem);

extern int neb_sem_notify_signal(neb_sem_t sem)
	__attribute_warn_unused_result__;
extern int neb_sem_notify_timedwait(neb_sem_t sem, const struct timespec *abs_timeout)
	__attribute_warn_unused_result__ neb_attr_nonnull((2));

/*
 * Other Posix Semaphore
 */

#endif
