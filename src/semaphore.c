
#include <nebase/syslog.h>
#include <nebase/semaphore.h>

#if defined(OS_DARWIN)
# include <dispatch/dispatch.h>
#else
# include <stdlib.h>
# include <errno.h>
# include <semaphore.h>
#endif

neb_sem_t neb_sem_notify_create(unsigned int value)
{
#if defined(OS_DARWIN)
	dispatch_semaphore_t sem = dispatch_semaphore_create(value);
	if (!sem) {
		neb_syslog(LOG_ERR, "dispatch_semaphore_create: failed");
		return NULL;
	}
#else
	sem_t *sem = malloc(sizeof(sem_t));
	if (sem_init(sem, 0, value) == -1) {
		neb_syslog(LOG_ERR, "sem_init: %m");
		free(sem);
		return NULL;
	}
#endif
	return (neb_sem_t)sem;
}

void neb_sem_notify_destroy(neb_sem_t sem)
{
#if defined(OS_DARWIN)
	dispatch_release((dispatch_semaphore_t)sem);
#else
	if (sem_destroy((sem_t *)sem) == -1)
		neb_syslog(LOG_ERR, "sem_destroy: %m");
	free(sem);
#endif
}

int neb_sem_notify_signal(neb_sem_t sem)
{
#if defined(OS_DARWIN)
	if (dispatch_semaphore_signal((dispatch_semaphore_t)sem) == 0) {
		neb_syslog(LOG_ERR, "dispatch_semaphore_signal: no thread is waiting");
		return -1;
	}
#else
	if (sem_post((sem_t *)sem) == -1) {
		neb_syslog(LOG_ERR, "sem_post: %m");
		return -1;
	}
#endif
	return 0;
}

int neb_sem_notify_timedwait(neb_sem_t sem, const struct timespec *abs_timeout)
{
#if defined(OS_DARWIN)
	dispatch_time_t dt = dispatch_walltime(abs_timeout, 0);
	if (dispatch_semaphore_wait((dispatch_semaphore_t)sem, dt)) {
		neb_syslog(LOG_ERR, "dispatch_semaphore_wait: timeout");
		return -1;
	}
#else
	for (;;) {
		if (sem_timedwait(sem, abs_timeout) == 0)
			return 0;
		if (errno != EINTR) {
			neb_syslog(LOG_ERR, "sem_timedwait: %m");
			return -1;
		}
	}
#endif
	return 0;
}
