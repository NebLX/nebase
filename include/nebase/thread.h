
#ifndef NEB_THREAD_H
#define NEB_THREAD_H 1

#include "cdefs.h"

#include <sys/types.h>

/*
 * Util Functions
 */

extern pid_t neb_thread_getid(void);
extern void neb_thread_setname(const char *name)
	_nattr_nonnull((1));

/*
 * The following is optional, but must be used together
 */

#include <pthread.h>

/* per process, do not fork after */
extern int neb_thread_init(void)
	_nattr_warn_unused_result;
extern void neb_thread_deinit(void);
/* per thread */
extern int neb_thread_register(void)
	_nattr_warn_unused_result;
extern int neb_thread_set_ready(void)
	_nattr_warn_unused_result;
/* in main thread */
extern int neb_thread_create(pthread_t *ptid, const pthread_attr_t *attr,
                             void *(*start_routine) (void *), void *arg)
	_nattr_warn_unused_result _nattr_nonnull((1, 3));
extern int neb_thread_is_running(pthread_t ptid)
	_nattr_warn_unused_result;
extern int neb_thread_destroy(pthread_t ptid, int kill_signo, void **retval)
	_nattr_warn_unused_result;

#endif
