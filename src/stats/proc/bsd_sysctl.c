
#include "options.h"

#include <nebase/stats/proc.h>
#include <nebase/syslog.h>
#include <nebase/sysconf.h>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <stddef.h>

#if defined(OS_FREEBSD)
# include <sys/user.h>
#endif

int neb_stats_proc_fill(pid_t pid, struct neb_stats_proc *s, int flags)
{
	s->pid = pid;
	int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};
	struct kinfo_proc kp;
	size_t len = sizeof(kp);
	if (sysctl(mib, 4, &kp, &len, NULL, 0) == -1) {
		neb_syslogl(LOG_ERR, "sysctl(kern.proc.pid.%d): %m", pid);
		return -1;
	}

#if defined(OS_FREEBSD)
	if (flags & NEB_PROC_F_VM)
		s->vm_rssize = kp.ki_rssize * neb_sysconf_pagesize;
	if (flags & NEB_PROC_F_START)
		TIMEVAL_TO_TIMESPEC(&kp.ki_start, &s->ts_start);
	if (flags & NEB_PROC_F_CPU) {
		s->tv_utime.tv_sec = kp.ki_rusage.ru_utime.tv_sec;
		s->tv_utime.tv_usec = kp.ki_rusage.ru_utime.tv_usec;
		s->tv_stime.tv_sec = kp.ki_rusage.ru_stime.tv_sec;
		s->tv_stime.tv_usec = kp.ki_rusage.ru_stime.tv_usec;
	}
#elif defined(OS_OPENBSD)
	if (flags & NEB_PROC_F_VM)
		s->vm_rssize = kp.p_vm_rssize * neb_sysconf_pagesize;
	if (flags & NEB_PROC_F_START) {
		s->ts_start.tv_sec = kp.p_ustart_sec;
		s->ts_start.tv_nsec = kp.p_ustart_usec * 1000;
	}
	if (flags & NEB_PROC_F_CPU) {
		s->tv_utime.tv_sec = kp.p_uutime_sec;
		s->tv_utime.tv_usec = kp.p_uutime_usec;
		s->tv_stime.tv_sec = kp.p_ustime_sec;
		s->tv_stime.tv_usec = kp.p_ustime_usec;
	}
#else
# error "fix me"
#endif

	return 0;
}
