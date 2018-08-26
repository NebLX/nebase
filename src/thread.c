
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/thread.h>

#include <unistd.h>

#if defined(OS_LINUX)
# include <sys/syscall.h>
#elif defined(OS_FREEBSD) || defined(OS_DRAGONFLY)
# include <pthread_np.h>
#elif defined(OS_NETBSD)
# include <lwp.h>
#elif defined(OS_SOLARIS)
# include <sys/lwp.h>
#endif

pid_t neb_thread_getid(void)
{
#if defined(OS_LINUX)
	return syscall(SYS_gettid);
#elif defined(OS_FREEBSD) || defined(OS_DRAGONFLY)
	return pthread_getthreadid_np();
#elif defined(OS_NETBSD) || defined(OS_SOLARIS)
	return _lwp_self();
#elif defined(OS_OPENBSD)
	return getthrid();
#else
# error "fixme"
#endif
}
