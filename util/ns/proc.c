
#include <nebase/stats/proc.h>
#include <nebase/time.h>

#include "main.h"

#include <unistd.h>
#include <stdio.h>

void print_stats_proc(pid_t pid)
{
	if (pid <= 0)
		pid = getpid();

	struct neb_stats_proc p;
	if (neb_stats_proc_fill(pid, &p, NEB_PROC_F_ALL) != 0) {
		fprintf(stderr, "failed to get proc stats for pid %d\n", pid);
		return;
	}

	fprintf(stdout, "proc stats for %d:\n", pid);
	fprintf(stdout, "Started at %llds%ldns\n",
		neb_time_sec_ll(p.ts_start.tv_sec), neb_time_nsec_l(p.ts_start.tv_nsec));
	fprintf(stdout, "CPU time: user %llds%ldus sys %llds%ldus\n",
		neb_time_sec_ll(p.tv_utime.tv_sec), neb_time_usec_l(p.tv_utime.tv_usec),
		neb_time_sec_ll(p.tv_stime.tv_sec), neb_time_usec_l(p.tv_stime.tv_usec));
	fprintf(stdout, "VM: size %zu rss %lu\n", p.vm_size, p.vm_rssize);
}
