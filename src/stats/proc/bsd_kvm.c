
#include "options.h"

#include <nebase/stats/proc.h>
#include <nebase/syslog.h>
#include <nebase/sysconf.h>

#include <sys/param.h>
#include <sys/sysctl.h>
#include <kvm.h>
#include <fcntl.h>
#include <limits.h>

#if defined(OS_FREEBSD)
# include <sys/user.h>
#endif

int neb_stats_proc_fill(pid_t pid, struct neb_stats_proc *s, int flags)
{
	s->pid = pid;

	int cnt, ret = 0;
#if defined(OS_FREEBSD)
	char errbuf[_POSIX2_LINE_MAX];
	kvm_t *kh = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf);
	if (!kh) {
		neb_syslogl(LOG_ERR, "kvm_openfiles: %s", errbuf);
		return -1;
	}

	struct kinfo_proc *kp = kvm_getprocs(kh, KERN_PROC_PID, pid, &cnt);
	if (!kp) {
		neb_syslogl(LOG_ERR, "kvm_getprocs: %s", kvm_geterr(kh));
		ret = -1;
	} else {
		if (flags & NEB_PROC_F_START) {
			s->ts_start.tv_sec = kp->ki_start.tv_sec;
			s->ts_start.tv_nsec = kp->ki_start.tv_usec * 1000;
		}
		if (flags & NEB_PROC_F_CPU) {
			const struct rusage *ru = &kp->ki_rusage;
			s->tv_utime.tv_sec = ru->ru_utime.tv_sec;
			s->tv_utime.tv_usec = ru->ru_utime.tv_usec;
			s->tv_stime.tv_sec = ru->ru_stime.tv_sec;
			s->tv_stime.tv_usec = ru->ru_stime.tv_usec;
		}
		if (flags & NEB_PROC_F_VM) {
			// sync with top(1) SIZE col
			s->vm_size = kp->ki_size; // in bytes
			// sync with top(1) RES col
			s->vm_rssize = kp->ki_rssize * neb_sysconf_pagesize; // in pages
		}
	}
#elif defined(OS_NETBSD)
	char errbuf[_POSIX2_LINE_MAX];
	kvm_t *kh = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
	if (!kh) {
		neb_syslogl(LOG_ERR, "kvm_openfiles: %s", errbuf);
		return -1;
	}

	struct kinfo_proc2 *kp = kvm_getproc2(kh, KERN_PROC_PID, pid, sizeof(struct kinfo_proc2), &cnt);
	if (!kp) {
		neb_syslogl(LOG_ERR, "kvm_getproc2: %s", kvm_geterr(kh));
		ret = -1;
	} else {
		if (flags & NEB_PROC_F_START) {
			s->ts_start.tv_sec = kp->p_ustart_sec;
			s->ts_start.tv_nsec = kp->p_ustart_usec * 1000;
		}
		if (flags & NEB_PROC_F_CPU) {
			s->tv_utime.tv_sec = kp->p_uutime_sec;
			s->tv_utime.tv_usec = kp->p_uutime_usec;
			s->tv_stime.tv_sec = kp->p_ustime_sec;
			s->tv_stime.tv_usec = kp->p_ustime_usec;
		}
		if (flags & NEB_PROC_F_VM) {
			// use msize for vm size, sync with top(1) SIZE col
			s->vm_size = kp->p_vm_msize * neb_sysconf_pagesize;// in pages
			// sync with top(1) RES col
			s->vm_rssize = kp->p_vm_rssize * neb_sysconf_pagesize; // in pages
		}
	}
#elif defined(OS_OPENBSD)
	char errbuf[_POSIX2_LINE_MAX];
	kvm_t *kh = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
	if (!kh) {
		neb_syslogl(LOG_ERR, "kvm_openfiles: %s", errbuf);
		return -1;
	}

	struct kinfo_proc *kp = kvm_getprocs(kh, KERN_PROC_PID, pid, sizeof(struct kinfo_proc), &cnt);
	if (!kp) {
		neb_syslogl(LOG_ERR, "kvm_getprocs: %s", kvm_geterr(kh));
		ret = -1;
	} else {
		if (flags & NEB_PROC_F_START) {
			s->ts_start.tv_sec = kp->p_ustart_sec;
			s->ts_start.tv_nsec = kp->p_ustart_usec * 1000;
		}
		if (flags & NEB_PROC_F_CPU) {
			s->tv_utime.tv_sec = kp->p_uutime_sec;
			s->tv_utime.tv_usec = kp->p_uutime_usec;
			s->tv_stime.tv_sec = kp->p_ustime_sec;
			s->tv_stime.tv_usec = kp->p_ustime_usec;
		}
		if (flags & NEB_PROC_F_VM) {
			// sync with ps(1) VSZ & top(1) SIZE col
			s->vm_size = (kp->p_vm_dsize + kp->p_vm_ssize + kp->p_vm_tsize) * neb_sysconf_pagesize; // in pages
			// sync with ps(1) RSS & top(1) RES col
			s->vm_rssize = kp->p_vm_rssize * neb_sysconf_pagesize; // in pages
		}
	}
#else
# error "fix me"
#endif

	if (kvm_close(kh) == -1)
		neb_syslogl(LOG_ERR, "kvm_close: %m");

	return ret;
}
