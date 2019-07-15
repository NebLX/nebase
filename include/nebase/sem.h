
#ifndef NEB_SEM_H
#define NEB_SEM_H 1

#include "cdefs.h"

#include <time.h>

typedef void * neb_sem_t;

/**
 * Notify Semaphore for inter-thread usage
 *   For waiting for a signal in one process
 */

extern neb_sem_t neb_sem_notify_create(unsigned int value)
	_nattr_warn_unused_result;
extern void neb_sem_notify_destroy(neb_sem_t sem);

extern int neb_sem_notify_signal(neb_sem_t sem)
	_nattr_warn_unused_result;
extern int neb_sem_notify_timedwait(neb_sem_t sem, const struct timespec *abs_timeout)
	_nattr_warn_unused_result _nattr_nonnull((2));

/**
 * Proc Semaphore for inter-process usage
 */

/**
 * \param[in] path NULL is allowed to create a private one
 *                 if not NULL, it should be a file with no sem associated
 */
extern int neb_sem_proc_create(const char *path, int nsems)
	_nattr_warn_unused_result;
extern int neb_sem_proc_destroy(int semid);

extern int neb_sem_proc_setval(int semid, int subid, int value);
/**
 * \return -1 if error, excluding EINTR
 */
extern int neb_sem_proc_post(int semid, int subid);

/**
 * \param[in] count should be greater than zero
 * \return -1 if error, including EINTR
 */
extern int neb_sem_proc_wait_count(int semid, int subid, int count, struct timespec *timeout)
	_nattr_warn_unused_result _nattr_nonnull((4));
/**
 * \return -1 if error, including EINTR
 */
extern int neb_sem_proc_wait_zerod(int semid, int subid, struct timespec *timeout)
	_nattr_warn_unused_result _nattr_nonnull((3));
/**
 * \param[in] subid should be an zero valued and unused subid
 * \note signal may be blocked before calling this, or it will be interrupted
 */
extern int neb_sem_proc_wait_removed(int semid, int subid, struct timespec *timeout)
	_nattr_warn_unused_result _nattr_nonnull((3));

/*
 * Other Semaphore
 */

#endif
