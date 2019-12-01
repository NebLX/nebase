
#include <nebase/stats/proc.h>
#include <nebase/syslog.h>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/task_info.h>
#include <mach/vm_region.h>
#include <mach/shared_memory_server.h>
#include <stddef.h>

/*
 * refer: adv_cmds/ps
 */

static int get_starttime(pid_t pid, struct timespec *tp)
{
	int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};
	struct kinfo_proc kp;
	size_t len = sizeof(kp);
	if (sysctl(mib, 4, &kp, &len, NULL, 0) == -1) {
		neb_syslogl(LOG_ERR, "sysctl(kern.proc.pid.%d): %m", pid);
		return -1;
	}

	TIMEVAL_TO_TIMESPEC(&kp.kp_proc.p_starttime, tp);
	return 0;
}

static int get_task_info(pid_t pid, struct neb_stats_proc *s, int flags)
{
	kern_return_t error;
	task_port_t task;
	error = task_for_pid(mach_task_self(), pid, &task);
	if (error != KERN_SUCCESS) {
		if ((error & system_emask) == err_kern) {
			neb_syslogl_en(err_get_code(error), LOG_ERR, "task_for_pid(%d): %s: %m", pid, mach_error_string(error));
		} else {
			neb_syslogl(LOG_ERR, "task_for_pid(%d): %s", pid, mach_error_string(error));
		}
		return -1;
	}

	unsigned int info_count = MACH_TASK_BASIC_INFO_COUNT;
	struct mach_task_basic_info info;
	error = task_info(task, MACH_TASK_BASIC_INFO, (task_info_t)&info, &info_count);
	if (error != KERN_SUCCESS) {
		neb_syslogl(LOG_ERR, "task_info: %s", mach_error_string(error));
		mach_port_deallocate(mach_task_self(), task);
		return -1;
	}

	if (flags & NEB_PROC_F_VM) {
		vm_region_basic_info_data_64_t b_info;
		vm_address_t address = GLOBAL_SHARED_TEXT_SEGMENT;
		vm_size_t size;
		mach_port_t object_name;

		/*
		 * try to determine if this task has the split libraries
		 * mapped in... if so, adjust its virtual size down by
		 * the 2 segments that are used for split libraries
		 */
		info_count = VM_REGION_BASIC_INFO_COUNT_64;
		error = vm_region_64(task, &address, &size, VM_REGION_BASIC_INFO,
		                     (vm_region_info_t)&b_info, &info_count, &object_name);
		if (error == KERN_SUCCESS) {
			if (b_info.reserved && size == (SHARED_TEXT_REGION_SIZE) &&
			    info.virtual_size > (SHARED_TEXT_REGION_SIZE + SHARED_DATA_REGION_SIZE))
				info.virtual_size -= (SHARED_TEXT_REGION_SIZE + SHARED_DATA_REGION_SIZE);
		}

		// sync with ps(1) VSZ col
		s->vm_size = info.virtual_size; // in bytes
		// sync with ps(1) RSS col
		s->vm_rssize = info.resident_size; // in bytes
	}
	if (flags & NEB_PROC_F_CPU) {
		s->tv_utime.tv_sec = info.user_time.seconds;
		s->tv_utime.tv_usec = info.user_time.microseconds;
		s->tv_stime.tv_sec = info.system_time.seconds;
		s->tv_stime.tv_usec = info.system_time.microseconds;
	}

	mach_port_deallocate(mach_task_self(), task);
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
		if (get_task_info(pid, s, flags) != 0)
			return -1;
	}

	return 0;
}
