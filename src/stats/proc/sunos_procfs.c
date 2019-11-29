
#include <nebase/stats/proc.h>
#include <nebase/syslog.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <procfs.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static int get_usage(pid_t pid, struct neb_stats_proc *s)
{
	char usage_file[32];
	snprintf(usage_file, sizeof(usage_file), "/proc/%d/usage", pid);

	int fd = open(usage_file, O_RDONLY | O_CLOEXEC);
	if (fd == -1) {
		neb_syslogl(LOG_ERR, "open(%s): %m", usage_file);
		return -1;
	}

	int ret = 0;
	struct prusage pu;
	ssize_t nr = read(fd, &pu, sizeof(pu));
	if (nr == -1) {
		neb_syslogl(LOG_ERR, "read: %m");
		ret = -1;
	} else {
		s->tv_utime.tv_sec = pu.pr_utime.tv_sec;
		s->tv_utime.tv_usec = pu.pr_utime.tv_nsec / 1000;
		s->tv_stime.tv_sec = pu.pr_stime.tv_sec;
		s->tv_stime.tv_usec = pu.pr_stime.tv_nsec / 1000;
	}
	close(fd);

	return ret;
}

static int get_psinfo(pid_t pid, struct neb_stats_proc *s, int flags)
{
	char psinfo_file[32]; // /proc/<pid>/psinfo
	snprintf(psinfo_file, sizeof(psinfo_file), "/proc/%d/psinfo", pid);

	int fd = open(psinfo_file, O_RDONLY | O_CLOEXEC);
	if (fd == -1) {
		neb_syslogl(LOG_ERR, "open(%s): %m", psinfo_file);
		return -1;
	}

	int ret = 0;
	struct psinfo pi;
	ssize_t nr = read(fd, &pi, sizeof(pi));
	if (nr == -1) {
		neb_syslogl(LOG_INFO, "read: %m");
		ret = -1;
	} else {
		if (flags & NEB_PROC_F_START) {
			s->ts_start.tv_sec = pi.pr_start.tv_sec;
			s->ts_start.tv_nsec = pi.pr_start.tv_nsec;
		}
		if (flags & NEB_PROC_F_VM)
			s->vm_rssize = pi.pr_rssize << 10; // pr_rssize is in kbytes
	}
	close(fd);

	return ret;
}

int neb_stats_proc_fill(pid_t pid, struct neb_stats_proc *s, int flags)
{
	s->pid = pid;
	if (flags & NEB_PROC_F_CPU) {
		if (get_usage(pid, s) != 0)
			return -1;
	}
	if (flags & (NEB_PROC_F_START | NEB_PROC_F_VM)) {
		if (get_psinfo(pid, s, flags) != 0)
			return -1;
	}
	return 0;
}
