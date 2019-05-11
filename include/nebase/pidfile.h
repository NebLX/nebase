
#ifndef NEB_PIDFILE_H
#define NEB_PIDFILE_H 1

#include "cdefs.h"

#include <sys/types.h>

/**
 * \param[out] locker <=0 if error, or the found pid
 * \return -1 if error or blocked, check *locker for locker pid
 *         or the opened pidfile fd
 * \note the returned fd will be cloexec, and may be used after fork
 */
extern int neb_pidfile_open(const char *path, pid_t *locker)
	_nattr_warn_unused_result;
/**
 * \return 0 if ok, < 0 if failed, or the locker pid
 * \note use this only in the real daemon process (after fork)
 */
extern pid_t neb_pidfile_write(int fd)
	_nattr_warn_unused_result;
/**
 * \brief close pidfile-fd before exit or after fork
 */
extern void neb_pidfile_close(int fd);
extern void neb_pidfile_remove(const char *path);

/**
 * \param[out] locker <=0 if error, or the found pid
 * \return -1 if error or blocked, check *locker for locker pid
 *         or the opened pidfile fd
 * \note the returned fd will be cloexec, and should be closed after fork
 */
extern int neb_pidlock(int dirfd, const char *filename, pid_t *locker)
	_nattr_warn_unused_result _nattr_nonnull((2));

#endif
