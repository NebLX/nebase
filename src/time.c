
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/time.h>

#if defined(OS_LINUX)
# include <sys/sysinfo.h>
/*
 * TODO use <bsd/sys/time.h> instead if we have libbsd-dev >= 0.8.4 on Linux
 */
# ifndef timespecsub
# define timespecsub(tsp, usp, vsp)                                      \
        do {                                                            \
                (vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec;          \
                (vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec;       \
                if ((vsp)->tv_nsec < 0) {                               \
                        (vsp)->tv_sec--;                                \
                        (vsp)->tv_nsec += 1000000000L;                  \
                }                                                       \
        } while (0)
# endif
#elif defined(OS_FREEBSD) || defined(OS_DFLYBSD) || defined(OS_DARWIN)
# include <sys/types.h>
# include <sys/sysctl.h>
#elif defined(OS_NETBSD)
# include <sys/param.h>
# include <sys/sysctl.h>
#elif defined(OS_OPENBSD)
# include <sys/types.h>
# include <sys/sysctl.h>
# include <sys/time.h>
#elif defined(OS_SOLARIS)
# include <kstat2.h>
#elif defined(OS_HAIKU)
# include <kernel/OS.h>
#endif

time_t neb_time_up(void)
{
#if defined(OS_LINUX)
	struct sysinfo si;
	if (sysinfo(&si) == -1) {
		neb_syslog(LOG_ERR, "sysinfo: %m");
		return 0;
	}
	return si.uptime;
#elif defined(OS_HAIKU)
	bigtime_t boot_usec = system_time();
	if (!boot_usec) {
		neb_syslog(LOG_ERR, "system_time: %m");
		return 0;
	}
	return boot_usec / 1000000;
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN) || defined(OS_SOLARIS)
	time_t boot = neb_time_boot();
	if (!boot)
		return 0;

	return time(NULL) - boot;
#else
# error "fix me"
#endif
}

time_t neb_time_boot(void)
{
#if defined(OS_LINUX) || defined(OS_HAIKU)
	time_t up = neb_time_up();
	if (!up)
		return 0;

	time_t boot = time(NULL) - up;
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	int name[2] = {CTL_KERN, KERN_BOOTTIME};
	struct timeval tv;
	size_t len = sizeof(tv);
	if (sysctl(name, 2, &tv, &len, NULL, 0) == -1) {
		neb_syslog(LOG_ERR, "sysctl: %m");
		return 0;
	}
	time_t boot = tv.tv_sec;
#elif defined(OS_SOLARIS)
	time_t boot = 0;
	kstat2_status_t stat;
	kstat2_matcher_list_t matchers;
	kstat2_handle_t handle;
	stat = kstat2_alloc_matcher_list(&matchers);
	if (stat != KSTAT2_S_OK) {
		neb_syslog(LOG_ERR, "kstat2_alloc_matcher_list: %s", kstat2_status_string(stat));
		return 0;
	}

	stat = kstat2_add_matcher(KSTAT2_M_STRING, "kstat:/misc/unix/system_misc", matchers);
	if (stat != KSTAT2_S_OK) {
		neb_syslog(LOG_ERR, "kstat2_add_matcher: %s", kstat2_status_string(stat));
		goto exit_free_matcher;
	}

	stat = kstat2_open(&handle, matchers);
	if (stat != KSTAT2_S_OK) {
		neb_syslog(LOG_ERR, "kstat2_open: %s", kstat2_status_string(stat));
		goto exit_free_matcher;
	}

	stat = kstat2_update(handle);
	if (stat != KSTAT2_S_OK) {
		neb_syslog(LOG_ERR, "kstat2_update: %s", kstat2_status_string(stat));
		goto exit_close_kstat2;
	}

	kstat2_map_t map; // a ref pointer
	stat = kstat2_lookup_map(handle, "kstat:/misc/unix/system_misc", &map);
	if (stat != KSTAT2_S_OK) {
		neb_syslog(LOG_ERR, "kstat2_lookup_map(kstat:/misc/unix/system_misc): %s", kstat2_status_string(stat));
		goto exit_close_kstat2;
	}

	kstat2_nv_t val;
	stat = kstat2_map_get(map, "boot_time", &val);
	if (stat != KSTAT2_S_OK) {
		neb_syslog(LOG_ERR, "kstat2_map_get(boot_time): %s", kstat2_status_string(stat));
		goto exit_close_kstat2;
	}
	if (val->type != KSTAT2_NVVT_INT) {
		neb_syslog(LOG_ERR, "Value of boot_time is not int");
		goto exit_close_kstat2;
	}

	boot = val->data.integer;

exit_close_kstat2:
	kstat2_close(&handle);
	exit_free_matcher:
	kstat2_free_matcher_list(&matchers);
#else
# error "fix me"
#endif
	return boot;
}

int neb_daytime_abs_nearest(int sec_of_day, time_t *abs_ts, int *delta_sec)
{
	time_t time_curr = time(NULL);
	if (time_curr == (time_t) -1) {
		neb_syslog(LOG_ERR, "time: %m");
		return -1;
	}

	struct tm tm_v;
	if (localtime_r(&time_curr, &tm_v) == NULL) {
		neb_syslog(LOG_ERR, "localtime_r: %m");
		return -1;
	}
	tm_v.tm_hour = sec_of_day / 3600;
	tm_v.tm_min = (sec_of_day % 3600) / 60;
	tm_v.tm_sec = (sec_of_day % 3600) % 60;

	time_t time_alarm = mktime(&tm_v);
	if (time_alarm == (time_t) -1) {
		neb_syslog(LOG_ERR, "mktime: %m");
		return -1;
	}

	while (time_alarm <= time_curr)
		time_alarm += 24 * 60 * 60;

	*abs_ts = time_alarm;
	time_t delta = time_alarm - time_curr;
	*delta_sec = delta;
	return 0;
}

int64_t neb_time_get_msec(void)
{
	static struct timespec init_ts = {.tv_sec = 0, .tv_nsec = 0};
	struct timespec ts;
#if defined(OS_LINUX)
	if (clock_gettime(CLOCK_MONOTONIC_COARSE, &ts) == -1) {
#elif defined(OS_FREEBSD) || defined(OS_DFLYBSD)
	if (clock_gettime(CLOCK_MONOTONIC_FAST, &ts) == -1) {
#elif defined(OS_OPENBSD) || defined(OS_SOLARIS)
	if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
#else
# error "fix me"
#endif
		neb_syslog(LOG_ERR, "clock_gettime: %m");
		return -1;
	}
	if (init_ts.tv_sec == 0) {
		init_ts.tv_sec = ts.tv_sec;
		init_ts.tv_nsec = ts.tv_nsec;
		return 0;
	} else {
		struct timespec diff_ts;
		timespecsub(&ts, &init_ts, &diff_ts);
		return diff_ts.tv_sec * 1000 + diff_ts.tv_nsec / 1000000;
	}
}
