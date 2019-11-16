
#include "options.h"

#include <nebase/syslog.h>

#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <limits.h>

#ifdef WITH_SYSTEMD
# ifndef SD_JOURNAL_SUPPRESS_LOCATION
#  define SD_JOURNAL_SUPPRESS_LOCATION
# endif
# include <systemd/sd-journal.h>
#endif

#ifdef WITH_GLIB2
# include <glib.h>
#endif

#define FMT_SIZE (1024 + 1)

_Thread_local pid_t thread_pid = 0;

const char neb_log_pri_symbol[] = {
	[LOG_EMERG  ] = 'S',
	[LOG_ALERT  ] = 'A',
	[LOG_CRIT   ] = 'C',
	[LOG_ERR    ] = 'E',
	[LOG_WARNING] = 'W',
	[LOG_NOTICE ] = 'N',
	[LOG_INFO   ] = 'I',
	[LOG_DEBUG  ] = 'D',
};
const char *neb_log_tty_color[] = {
	[LOG_EMERG  ] = "\e[3;31m!!",    // italic red
	[LOG_ALERT  ] = "\e[3;31m!!",    // italic red
	[LOG_CRIT   ] = "\e[3;31m!!",    // italic red
	[LOG_ERR    ] = "\e[31m?!",      // red
	[LOG_WARNING] = "\e[35m?=",      // magenta
	[LOG_NOTICE ] = "\e[33m==",      // yellow
	[LOG_INFO   ] = "\e[37m--",      // white
	[LOG_DEBUG  ] = "\e[37m##",      // white
};
#ifdef WITH_GLIB2
const int neb_log_glog_flag[] = {
	[LOG_EMERG  ] = G_LOG_LEVEL_ERROR,
	[LOG_ALERT  ] = G_LOG_LEVEL_ERROR,
	[LOG_CRIT   ] = G_LOG_LEVEL_ERROR,
	[LOG_ERR    ] = G_LOG_LEVEL_CRITICAL,
	[LOG_WARNING] = G_LOG_LEVEL_WARNING,
	[LOG_NOTICE ] = G_LOG_LEVEL_MESSAGE,
	[LOG_INFO   ] = G_LOG_LEVEL_INFO,
	[LOG_DEBUG  ] = G_LOG_LEVEL_DEBUG,
};
#endif

static void log_to_null(const char *d _nattr_unused,
                        int pri _nattr_unused,
                        const char *fmt _nattr_unused,
                        va_list ap _nattr_unused)
{
	return;
}

int neb_syslog_max_priority = LOG_INFO;
int neb_syslog_facility = LOG_USER; // the same with os default
neb_custom_logger neb_syslog_custom_logger = log_to_null;

static const char *neb_syslog_domain = NULL;
static int neb_syslog_mask = LOG_UPTO(LOG_DEBUG);
static int neb_syslog_type = NEB_LOG_STDIO;

#if defined(OS_NETBSD) || defined(OS_OPENBSD)
static struct syslog_data sdata = SYSLOG_DATA_INIT;
#endif

const char *neb_syslog_default_domain(void)
{
#if defined(OS_LINUX)
	return program_invocation_short_name;
#else
	return getprogname();
#endif
}

int neb_syslog_init(int log_type, const char *domain)
{
	neb_syslog_type = log_type;
	neb_syslog_mask = LOG_UPTO(LOG_PRI(neb_syslog_max_priority));
	neb_syslog_domain = domain ? domain : neb_syslog_default_domain();
	switch (neb_syslog_type) {
	case NEB_LOG_SYSLOG:
#if defined(OS_NETBSD) || defined(OS_OPENBSD)
		openlog_r(neb_syslog_domain, LOG_CONS | LOG_PID, neb_syslog_facility, &sdata);
		setlogmask_r(neb_syslog_mask, &sdata);
#else
		openlog(neb_syslog_domain, LOG_CONS | LOG_PID, neb_syslog_facility);
		setlogmask(neb_syslog_mask);
#endif
		break;
	case NEB_LOG_STDIO:
		break;
	case NEB_LOG_JOURNALD:
#ifndef WITH_SYSTEMD
		return -1;
#endif
		break;
	case NEB_LOG_GLOG:
#ifndef WITH_GLIB2
		return -1;
#endif
		// do nothing, let users set it up
		break;
	case NEB_LOG_CUSTOM:
		break;
	default:
		return -1;
		break;
	}
	return 0;
}

void neb_syslog_deinit(void)
{
	switch (neb_syslog_type) {
	case NEB_LOG_SYSLOG:
#if defined(OS_NETBSD) || defined(OS_OPENBSD)
		closelog_r(&sdata);
#else
		closelog();
#endif
		break;
	default:
		break;
	}
}

#if defined(PRINTF_SUPPORT_STRERR) || defined(GLOG_SUPPORT_STRERR)
static void get_normalized_fmt(const char *fmt, char fmt_cpy[FMT_SIZE]) _nattr_unused;
#endif

static void get_normalized_fmt(const char *fmt, char fmt_cpy[FMT_SIZE])
{
	char ch, *t;
	int fmt_left, prlen, saved_errno;

	saved_errno = errno;

	for (t = fmt_cpy, fmt_left = FMT_SIZE;
	     (ch = *fmt) != '\0' && fmt_left > 1;
	     ++fmt) {
		if (ch == '%' && fmt[1] == 'm') {
			char ebuf[FMT_SIZE];

			++fmt;
			(void)strerror_r(saved_errno, ebuf, sizeof(ebuf));
			prlen = snprintf(t, fmt_left, "%s", ebuf);
			if (prlen < 0)
				prlen = 0;
			if (prlen >= fmt_left)
				prlen = fmt_left - 1;
			t += prlen;
			fmt_left -= prlen;
		} else if (ch == '%' && fmt[1] == '%' && fmt_left > 2) {
			++fmt;
			*t++ = '%';
			*t++ = '%';
			fmt_left -= 2;
		} else {
			*t++ = ch;
			fmt_left--;
		}
	}
	*t = '\0';
}

#if defined(WITH_GLIB2) && !defined(GLOG_SUPPORT_STRERR)
/**
 * \note only the first one of %m will be replaced
 */
static void glog_with_strerr(int pri, const char *fmt, va_list va)
{
	char fmt_cpy[FMT_SIZE];
	get_normalized_fmt(fmt, fmt_cpy);

	g_logv(neb_syslog_domain, neb_log_glog_flag[pri], fmt_cpy, va);
}
#endif

static void log_to_stdio(int pri, const char *fmt, va_list va)
{
	FILE *stream;
	switch (pri) {
	case LOG_DEBUG:
	case LOG_INFO:
	case LOG_NOTICE:
		stream = stdout;
		break;
	default:
		stream = stderr;
		break;
	}

	if (isatty(fileno(stream)))
		fprintf(stream, "%s\e[0m ", neb_log_tty_color[pri]);

	time_t ts = time(NULL);
	struct tm tm;
	localtime_r(&ts, &tm);
	char tdata[32];
	size_t len = strftime(tdata, sizeof(tdata), "%b %d %H:%M:%S", &tm);
	tdata[len] = '\0';

	fprintf(stream, "%s %s[%d]: ", tdata, neb_syslog_domain, getpid());
#ifdef PRINTF_SUPPORT_STRERR
	(void)vfprintf(stream, fmt, va);
#else
	char fmt_cpy[FMT_SIZE];
	get_normalized_fmt(fmt, fmt_cpy);

	(void)vfprintf(stream, fmt_cpy, va);
#endif
	fprintf(stream, "\n");
}

static inline void neb_do_vsyslog(int pri, const char *fmt, va_list va)
{
	switch (neb_syslog_type) {
	case NEB_LOG_SYSLOG:
#if defined(OS_NETBSD) || defined(OS_OPENBSD)
		vsyslog_r(LOG_MAKEPRI(neb_syslog_facility, pri), &sdata, fmt, va);
#else
		vsyslog(LOG_MAKEPRI(neb_syslog_facility, pri), fmt, va);
#endif
		break;
	case NEB_LOG_STDIO:
		log_to_stdio(pri, fmt, va);
		break;
	case NEB_LOG_JOURNALD:
#ifdef WITH_SYSTEMD
		sd_journal_printv(LOG_PRI(pri), fmt, va);
#endif
		break;
	case NEB_LOG_GLOG:
#ifdef WITH_GLIB2
# ifdef GLOG_SUPPORT_STRERR
		g_logv(neb_syslog_domain, neb_log_glog_flag[pri], fmt, va);
# else
		glog_with_strerr(pri, fmt, va);
# endif
#endif
		break;
	case NEB_LOG_CUSTOM:
		neb_syslog_custom_logger(neb_syslog_domain, pri, fmt, va);
		break;
	default:
		break;
	}
}

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
