
#include "options.h"

#include <nebase/syslog.h>

#include <errno.h>

const char neb_log_pri_symbol[] = {
	[LOG_EMERG  ] = 'S',
	[LOG_ALERT  ] = 'A',
	[LOG_CRIT   ] = 'C',
	[LOG_ERR    ] = 'E',
	[LOG_WARNING] = 'W',
	[LOG_NOTICE ] = 'N',
	[LOG_INFO   ] = 'I',
	[LOG_DEBUG  ] = 'D'
};

int neb_syslog_max_priority = LOG_INFO;
static int neb_syslog_mask = LOG_UPTO(LOG_INFO);

void neb_syslog_init(void)
{
#if defined(OS_LINUX)
# ifndef WITH_SYSTEMD
	openlog(program_invocation_short_name, LOG_CONS | LOG_PID, LOG_DAEMON);
# endif
#elif defined(OS_FREEBSD)
	openlog(getprogname(), LOG_CONS | LOG_PID, LOG_DAEMON);
#else
# error "fix me"
#endif

	neb_syslog_mask = LOG_UPTO(LOG_PRI(neb_syslog_max_priority));
#ifndef WITH_SYSTEMD
	setlogmask(neb_syslog_mask);
#endif
}

void psd_syslog_deinit(void)
{
#ifndef WITH_SYSTEMD
	closelog();
#endif
}
