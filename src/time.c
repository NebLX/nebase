
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/time.h>

#include <time.h>

#if defined(OS_LINUX)
# include <sys/sysinfo.h>
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
#else
# error "fix me"
#endif
}

time_t neb_time_boot(void)
{
	time_t up = neb_time_up();
	if (!up)
		return 0;

	return time(NULL) - up;
}
