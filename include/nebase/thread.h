
#ifndef NEB_THREAD_H
#define NEB_THREAD_H 1

#include <sys/types.h>

/*
 * Util Functions
 */

extern pid_t neb_thread_getid(void);

/*
 * The following is optional, but must be used together
 */

#include <pthread.h>

/* per process */
extern int neb_thread_init(void);
extern void neb_thread_deinit(void);
/* per thread */
extern int neb_thread_register(void);
/* in main thread */
extern int neb_thread_is_running(pthread_t ptid);

#endif
