
#ifndef NEB_SYSLOG_H
#define NEB_SYSLOG_H 1

#include "cdefs.h"

#include <sys/types.h>
#include <syslog.h>

#ifndef LOG_PRIMASK
#define LOG_PRIMASK 0x07
#endif
#ifndef LOG_PRI
#define LOG_PRI(p) ((p) & LOG_PRIMASK)
#endif
#ifndef LOG_MAKEPRI
#define LOG_MAKEPRI(fac, pri) ((fac) | (pri))
#endif

extern _Thread_local pid_t thread_pid;

/* syslog backends */
enum {
	/* stdio, which is the default */
	NEB_LOG_STDIO,
	/* syslog */
	NEB_LOG_SYSLOG,
	/* systemd journald, which is available on linux */
	NEB_LOG_JOURNALD,
	/* glib2 glog */
	NEB_LOG_GLOG,
};

extern const char *neb_syslog_default_domain(void);
/**
 * \param[in] domain log domain, set to NULL to use default one
 *                   it will be NULL if this function is not called
 * \note If you use glog, initial it before call this function
 */
extern int neb_syslog_init(int log_type, const char *domain);
/**
 * optional
 */
extern void neb_syslog_deinit(void);

extern const char neb_log_pri_symbol[];
extern int neb_syslog_max_priority;
extern int neb_syslog_facility;
extern void neb_syslog_r(int priority, const char *format, ...)
	_nattr_nonnull((2)) __sysloglike(2, 3);
extern void neb_syslog_en_r(int err, int priority, const char *format, ...)
	_nattr_nonnull((3)) __sysloglike(3, 4);

#define neb_syslog(pri, fmt, ...)\
{\
	if (thread_pid)\
		neb_syslog_r(pri, "[%d][%c] "fmt, thread_pid, neb_log_pri_symbol[LOG_PRI(pri)], ##__VA_ARGS__);\
	else\
		neb_syslog_r(pri, "[%c] "fmt, neb_log_pri_symbol[LOG_PRI(pri)], ##__VA_ARGS__);\
}
#define neb_syslogl(pri, fmt, ...)\
{\
	if (thread_pid)\
		neb_syslog_r(pri, "[%d][%c] %s:%d "fmt, thread_pid, neb_log_pri_symbol[LOG_PRI(pri)], __FILE__, __LINE__, ##__VA_ARGS__);\
	else\
		neb_syslog_r(pri, "[%c] %s:%d "fmt, neb_log_pri_symbol[LOG_PRI(pri)], __FILE__, __LINE__, ##__VA_ARGS__);\
}
#define neb_syslog_en(err, pri, fmt, ...)\
{\
	if (thread_pid)\
		neb_syslog_en_r(err, pri, "[%d][%c] "fmt, thread_pid, neb_log_pri_symbol[LOG_PRI(pri)], ##__VA_ARGS__);\
	else\
		neb_syslog_en_r(err, pri, "[%c] "fmt, neb_log_pri_symbol[LOG_PRI(pri)], ##__VA_ARGS__);\
}
#define neb_syslogl_en(err, pri, fmt, ...)\
{\
	if (thread_pid)\
		neb_syslog_en_r(err, pri, "[%d][%c] %s:%d "fmt, thread_pid, neb_log_pri_symbol[LOG_PRI(pri)], __FILE__, __LINE__, ##__VA_ARGS__);\
	else\
		neb_syslog_en_r(err, pri, "[%c] %s:%d "fmt, neb_log_pri_symbol[LOG_PRI(pri)], __FILE__, __LINE__, ##__VA_ARGS__);\
}

#endif
