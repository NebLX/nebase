
#ifndef NEB_THREAD_H
#define NEB_THREAD_H 1

#include <sys/types.h>

/*
 * Util Functions
 */

extern pid_t neb_thread_getid(void);
extern void neb_thread_setname(const char *name)
	neb_attr_nonnull((1));

/*
 * The following is optional, but must be used together
 */

#include <pthread.h>

/* per process */
extern int neb_thread_init(void)
	__attribute_warn_unused_result__;
extern void neb_thread_deinit(void);
/* per thread */
extern int neb_thread_register(void)
	__attribute_warn_unused_result__;
extern int neb_thread_set_ready(void)
	__attribute_warn_unused_result__;
/* in main thread */
extern int neb_thread_create(pthread_t *ptid, const pthread_attr_t *attr,
                             void *(*start_routine) (void *), void *arg)
	__attribute_warn_unused_result__ neb_attr_nonnull((1, 3));
extern int neb_thread_is_running(pthread_t ptid)
	__attribute_warn_unused_result__;
extern int neb_thread_destroy(pthread_t ptid, int kill_signo, void **retval)
	__attribute_warn_unused_result__;

#endif
