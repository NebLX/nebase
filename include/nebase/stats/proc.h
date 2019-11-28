
#ifndef NEB_STATS_PROC_H
#define NEB_STATS_PROC_H 1

#include <nebase/cdefs.h>

#include <sys/types.h>
#include <sys/time.h>

#define NEB_PROC_F_START 0x01 // start time
#define NEB_PROC_F_VM    0x02 // memory
#define NEB_PROC_F_CPU   0x04 // cpu time
#define NEB_PROC_F_ALL   0xFF

struct neb_stats_proc {
	pid_t pid;
	struct timespec ts_start;
	struct timeval tv_utime;
	struct timeval tv_stime;
	size_t vm_rssize;
};

extern int neb_stats_proc_fill(pid_t pid, struct neb_stats_proc *s, int flags)
	_nattr_warn_unused_result _nattr_nonnull((2));

#endif
