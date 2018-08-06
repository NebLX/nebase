
#ifndef NEB_SYSLOG_H
#define NEB_SYSLOG_H 1

#include "cdefs.h"

#include <syslog.h>

extern void neb_syslog_init(void);
extern void neb_syslog_deinit(void);

extern const char neb_log_pri_symbol[];
extern int neb_syslog_max_priority;
extern void neb_syslog_r(int priority, const char *format, ...)
	neb_attr_nonnull((2)) __attribute__((__format__(printf, 2, 3)));
extern void neb_syslog_en_r(int err, int priority, const char *format, ...)
	neb_attr_nonnull((3)) __attribute__((__format__(printf, 3, 4)));

#define neb_syslog(pri, fmt, ...) \
	neb_syslog_r(pri, "[%c] "fmt, neb_log_pri_symbol[LOG_PRI(pri)], ##__VA_ARGS__)
#define neb_syslogl(pri, fmt, ...) \
	neb_syslog_r(pri, "[%c] %s:%d "fmt, neb_log_pri_symbol[LOG_PRI(pri)], __FILE__, __LINE__, ##__VA_ARGS__)
#define neb_syslog_en(err, pri, fmt, ...) \
	neb_syslog_en_r(err, pri, "[%c] "fmt, neb_log_pri_symbol[LOG_PRI(pri)], ##__VA_ARGS__)
#define neb_syslogl_en(err, pri, fmt, ...) \
	neb_syslog_en_r(err, pri, "[%c] %s:%d "fmt, neb_log_pri_symbol[LOG_PRI(pri)], __FILE__, __LINE__, ##__VA_ARGS__)

#endif
