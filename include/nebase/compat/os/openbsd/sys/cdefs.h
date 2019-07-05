
#ifndef NEB_COMPAT_SYS_CDEFS_H
#define NEB_COMPAT_SYS_CDEFS_H 1

#include_next <sys/cdefs.h>

#ifndef __sysloglike
# define __sysloglike(fmtarg, firstvararg) \
	__attribute__((__format__ (__syslog__, fmtarg, firstvararg)))
#endif

#endif
