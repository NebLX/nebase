
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/thread.h>

#include <unistd.h>

#if defined(OS_LINUX)
# include <sys/syscall.h>
#elif defined(OS_FREEBSD) || defined(OS_DFLYBSD)
# include <pthread_np.h>
#elif defined(OS_NETBSD)
# include <lwp.h>
#elif defined(OS_SOLARIS)
# include <sys/lwp.h>
#elif defined(OS_DARWIN)
# include <pthread.h>
#endif

pid_t neb_thread_getid(void)
{
#if defined(OS_LINUX)
	return syscall(SYS_gettid);
#elif defined(OS_FREEBSD) || defined(OS_DFLYBSD)
	return pthread_getthreadid_np();
#elif defined(OS_NETBSD) || defined(OS_SOLARIS)
	return _lwp_self();
#elif defined(OS_OPENBSD)
	return getthrid();
#elif defined(OS_DARWIN)
	uint64_t tid; //the system-wide unique integral ID of thread
	int ret = pthread_threadid_np(pthread_self(), &tid);
	if (ret != 0) {
		neb_syslog_en(ret, LOG_ERR, "pthread_threadid_np: %m");
		return 0;
	}
	return tid;
#else
# error "fixme"
#endif
}
