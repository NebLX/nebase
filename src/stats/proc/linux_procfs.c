
#include <nebase/stats/proc.h>
#include <nebase/syslog.h>
#include <nebase/sysconf.h>
#include <nebase/time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

static int get_starttime(pid_t pid, struct timespec *tp)
{
	char proc_dir[32]; // /proc/<pid>
	snprintf(proc_dir, sizeof(proc_dir), "/proc/%d", pid);

	int dirfd = open(proc_dir, O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOATIME | O_PATH);
	if (dirfd == -1) {
		neb_syslogl(LOG_ERR, "open(%s): %m", proc_dir);
		return -1;
	}

	struct stat s;
	int ret = fstatat(dirfd, "exe", &s, AT_SYMLINK_NOFOLLOW);
	if (ret == -1) {
		neb_syslogl(LOG_ERR, "fstatat(%s/exe): %m", proc_dir);
	} else {
		tp->tv_sec = s.st_mtim.tv_sec;
		tp->tv_nsec = s.st_mtim.tv_nsec;
	}
	close(dirfd);

	return ret;
}

static int get_stat(pid_t pid, struct neb_stats_proc *s, int flags)
{
	char stat_file[64]; // /proc/<pid>/stat
	snprintf(stat_file, sizeof(stat_file), "/proc/%d/stat", pid);

	int fd = open(stat_file, O_RDONLY);
	if (fd == -1) {
		neb_syslogl(LOG_ERR, "open(%s): %m", stat_file);
		return -1;
	}

	char buf[2048]; // should be large enough
	ssize_t nr = read(fd, buf, sizeof(buf));
	if (nr == sizeof(buf)) {
		neb_syslog(LOG_ERR, "%s: contains too much data", stat_file);
		close(fd);
		return -1;
	}
	buf[nr] = '\0';

	char *end_cmd = strrchr(buf, ')'); // end of column 2
	if (!end_cmd || *(end_cmd + 1) != ' ') {
		neb_syslog(LOG_ERR, "%s: no valid comm found", stat_file);
		close(fd);
		return -1;
	}
	int col = 2;
	char *col3 = end_cmd + 2;
	char *saveptr;
	for (char *str = strtok_r(col3, " ", &saveptr); str && *str; str = strtok_r(NULL, " ", &saveptr)) {
		col++;
		switch (col) {
		case 14: // utime
			if (flags & NEB_PROC_F_CPU) {
				unsigned long utime = strtoul(str, NULL, 10);
				neb_clktck2timeval(utime, &s->tv_utime);
			}
			break;
		case 15: // stime
			if (flags & NEB_PROC_F_CPU) {
				unsigned long stime = strtoul(str, NULL, 10);
				neb_clktck2timeval(stime, &s->tv_stime);
			}
			break;
		case 23: // vsize
			if (flags & NEB_PROC_F_VM) {
				// sync with top(1) VIRT col
				unsigned long vsize = strtoul(str, NULL, 10); // in bytes
				s->vm_size = vsize;
			}
			break;
		case 24: // rss
			if (flags & NEB_PROC_F_VM) {
				// sync with top(1) RES col
				long pages = strtol(str, NULL, 10); // in pages
				s->vm_rssize = pages * neb_sysconf_pagesize;
			}
			break;
		default:
			break;
		}
	}

	return 0;
}

int neb_stats_proc_fill(pid_t pid, struct neb_stats_proc *s, int flags)
{
	s->pid = pid;
	if (flags & NEB_PROC_F_START) {
		if (get_starttime(pid, &s->ts_start) != 0)
			return -1;
	}
	if (flags & (NEB_PROC_F_CPU | NEB_PROC_F_VM)) {
		if (get_stat(pid, s, flags) != 0)
			return -1;
	}

	return 0;
}
