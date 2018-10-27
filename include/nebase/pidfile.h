
#ifndef NEB_PIDFILE_H
#define NEB_PIDFILE_H 1

#include "cdefs.h"

#include <sys/types.h>

/**
 * \param[out] locker <=0 if error, or the found pid
 * \return -1 if error or blocked, check *locker for locker pid
 *         or the opened pidfile fd
 */
extern int neb_pidfile_open(const char *path, pid_t *locker)
	__attribute_warn_unused_result__;
/**
 * \return 0 if ok, < 0 if failed, or the locker pid
 */
extern pid_t neb_pidfile_write(int fd)
	__attribute_warn_unused_result__;
extern void neb_pidfile_close(int fd);
extern void neb_pidfile_remove(const char *path);

/**
 * \param[out] locker <=0 if error, or the found pid
 * \return -1 if error or blocked, check *locker for locker pid
 *         or the opened pidfile fd
 */
extern int neb_pidlock(int dirfd, const char *filename, pid_t *locker)
	__attribute_warn_unused_result__ neb_attr_nonnull((2));

#endif
