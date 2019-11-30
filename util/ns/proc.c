
#include <nebase/stats/proc.h>

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
	fprintf(stdout, "Started at %lds%ldns\n", p.ts_start.tv_sec, p.ts_start.tv_nsec);
	fprintf(stdout, "CPU time: user %lds%ldus sys %lds%ldus\n",
		p.tv_utime.tv_sec, p.tv_utime.tv_usec,
		p.tv_stime.tv_sec, p.tv_stime.tv_usec);
	fprintf(stdout, "VM: size %zu rss %lu\n", p.vm_size, p.vm_rssize);
}
