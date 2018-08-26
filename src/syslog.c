
#include "options.h"

#include <nebase/syslog.h>

#include <stdarg.h>
#include <errno.h>

#if defined(OS_FREEBSD) || defined(OS_NETBSD) || defined(OS_SOLARIS)
# include <stdlib.h>
#endif

#ifdef WITH_SYSTEMD
# include <systemd/sd-journal.h>
#endif

_Thread_local int thread_pid = 0;

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
#elif defined(OSTYPE_BSD) || defined(OS_SOLARIS)
	openlog(getprogname(), LOG_CONS | LOG_PID, LOG_DAEMON);
#else
# error "fix me"
#endif

	neb_syslog_mask = LOG_UPTO(LOG_PRI(neb_syslog_max_priority));
#ifndef WITH_SYSTEMD
	setlogmask(neb_syslog_mask);
#endif
}

void neb_syslog_deinit(void)
{
#ifndef WITH_SYSTEMD
	closelog();
#endif
}

#ifdef WITH_SYSTEMD
# define neb_do_vsyslog(pri, fmt, va) \
	sd_journal_printv(LOG_PRI(pri), fmt, va)
#else
# define neb_do_vsyslog(pri, fmt, va) \
	vsyslog(LOG_MAKEPRI(LOG_DAEMON, pri), fmt, va)
#endif

void neb_syslog_r(int priority, const char *format, ...)
{
	if (!(LOG_MASK(priority) & neb_syslog_mask))
		return;

	va_list ap;
	va_start(ap, format);
	neb_do_vsyslog(priority, format, ap);
	va_end(ap);
}

void neb_syslog_en_r(int err, int priority, const char *format, ...)
{
	if (!(LOG_MASK(priority) & neb_syslog_mask))
		return;

	va_list ap;
	va_start(ap, format);
	errno = err;
	neb_do_vsyslog(priority, format, ap);
	va_end(ap);
}
