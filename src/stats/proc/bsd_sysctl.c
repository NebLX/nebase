
#include "options.h"

#include <nebase/stats/proc.h>
#include <nebase/syslog.h>
#include <nebase/sysconf.h>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <stddef.h>

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
	if (flags & NEB_PROC_F_VM) {
		// sync with top(1) SIZE col
		s->vm_size = kp.ki_size; // in bytes
		// sync with top(1) RES col
		s->vm_rssize = kp.ki_rssize * neb_sysconf_pagesize; // in pages
	}
	if (flags & NEB_PROC_F_START)
		TIMEVAL_TO_TIMESPEC(&kp.ki_start, &s->ts_start);
	if (flags & NEB_PROC_F_CPU) {
		const struct rusage *ru = &kp.ki_rusage;
		s->tv_utime.tv_sec = ru->ru_utime.tv_sec;
		s->tv_utime.tv_usec = ru->ru_utime.tv_usec;
		s->tv_stime.tv_sec = ru->ru_stime.tv_sec;
		s->tv_stime.tv_usec = ru->ru_stime.tv_usec;
	}
#elif defined(OS_DFLYBSD)
	if (flags & NEB_PROC_F_VM) {
		// sync with top(1) SIZE col
		s->vm_size = kp.kp_vm_map_size; // in bytes
		// sync with top(1) RES col
		s->vm_rssize = kp.kp_vm_rssize * neb_sysconf_pagesize; // in pages
	}
	if (flags & NEB_PROC_F_START)
		TIMEVAL_TO_TIMESPEC(&kp.kp_start, &s->ts_start);
	if (flags & NEB_PROC_F_CPU) {
		const struct rusage *ru = &kp.kp_ru;
		s->tv_utime.tv_sec = ru->ru_utime.tv_sec;
		s->tv_utime.tv_usec = ru->ru_utime.tv_usec;
		s->tv_stime.tv_sec = ru->ru_stime.tv_sec;
		s->tv_stime.tv_usec = ru->ru_stime.tv_usec;
	}
#else
# error "fix me"
#endif

	return 0;
}
