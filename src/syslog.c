
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

#include <glib.h>

#ifdef WITH_SYSTEMD
# include <systemd/sd-journal.h>
#endif

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

int neb_syslog_max_priority = LOG_INFO;
int neb_syslog_facility = LOG_USER; // the same with os default

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

static void stdio_glog_handler(const gchar *log_domain, GLogLevelFlags log_level,
                               const gchar *message, gpointer unused_data _nattr_unused)
{
	FILE *stream = stdout;
	switch (log_level) {
	case G_LOG_LEVEL_ERROR:
	case G_LOG_LEVEL_CRITICAL:
	case G_LOG_LEVEL_WARNING:
		stream = stderr;
		break;
	default:
		break;
	}

	time_t ts = time(NULL);
	struct tm tm;
	localtime_r(&ts, &tm);
	char buf[32];
	size_t len = strftime(buf, sizeof(buf), "%b %d %H:%M:%S", &tm);
	buf[len] = '\0';

	fprintf(stream, "%s %s: %s\n", buf, log_domain, message);
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
		g_log_set_handler(neb_syslog_domain, G_LOG_LEVEL_MASK, stdio_glog_handler, NULL);
		break;
	case NEB_LOG_JOURNALD:
#ifndef WITH_SYSTEMD
		return -1;
#endif
		break;
	case NEB_LOG_GLOG:
		// do nothing, let users set it up
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

#ifndef GLOG_SUPPORT_STRERR
/**
 * \note only the first one of %m will be replaced
 */
static void glog_with_strerr(int pri, const char *fmt, va_list va)
{
	int off = -1;
	char *pattern = strstr(fmt, "%m");
	if (pattern)
		off = pattern - fmt;
	if (off == -1) {
		g_logv(neb_syslog_domain, neb_log_glog_flags[pri], fmt, va);
	} else {
		int len = strlen(fmt);
		int buf_len = len + LINE_MAX;
		char *buf = malloc(buf_len + 1);
		if (!buf) {
			g_log(neb_syslog_domain, G_LOG_LEVEL_CRITICAL, "malloc failed when trying to parse %%m");
			g_logv(neb_syslog_domain, neb_log_glog_flags[pri], fmt, va);
		} else {
			if (off > 0)
				memcpy(buf, fmt, off);
# if defined(OS_LINUX)
			if (!strerror_r(errno, buf + off, buf_len - off)) {
# else
			if (strerror_r(errno, buf + off, buf_len - off) != 0) {
# endif
				g_log(neb_syslog_domain, G_LOG_LEVEL_CRITICAL, "strerror_r failed when trying to parse %%m");
				g_logv(neb_syslog_domain, neb_log_glog_flags[pri], fmt, va);
				free(buf);
				return;
			}

			if (len > off + 2) {
				int next_off = -1;
				for (int i = off; i < buf_len; i++) {
					if (buf[i] == '\0') {
						next_off = i;
						break;
					}
				}
				// LINE_MAX is large enough to hold libc errors, so no overflow check here
				strncpy(buf + next_off, fmt + off + 2, buf_len - next_off);
				buf[buf_len] = '\0';
			}

			g_logv(neb_syslog_domain, neb_log_glog_flags[pri], buf, va);
			free(buf);
		}
	}
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

	fprintf(stream, "%s[%d]: ", neb_syslog_domain, getpid());
#ifdef PRINTF_SUPPORT_STRERR
	(void)vfprintf(stream, fmt, va);
#else
# define FMT_SIZE (1024+1)

	char ch, *t;
	char fmt_cpy[FMT_SIZE];
	int fmt_left, prelen;

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

	(void)vfprintf(stream, fmt_cpy, va);
#endif
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
#ifdef GLOG_SUPPORT_STRERR
		g_logv(neb_syslog_domain, neb_log_glog_flag[pri], fmt, va);
#else
		glog_with_strerr(pri, fmt, va);
#endif
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
