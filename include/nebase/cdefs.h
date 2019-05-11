
#ifndef NEB_CDEFS_H
#define NEB_CDEFS_H 1

// standard cdefs

#include "platform.h"

#ifndef __sysloglike
# ifndef __syslog_attribute__
#  define __syslog__ __printf__
# endif
# define __sysloglike(fmtarg, firstvararg) \
	__attribute__((__format__ (__syslog__, fmtarg, firstvararg)))
#endif

/*
 * NeBase specific cdefs
 */

#define _nattr_warn_unused_result __attribute__ ((__warn_unused_result__))

// unused param or function
#define _nattr_unused __attribute__((__unused__))

// non null param
#define _nattr_nonnull(x) __attribute__ ((__nonnull__ x))

// API deprecated
#define _nattr_deprecated __attribute__((__deprecated__))

// hidden symbol
#define _nattr_hidden __attribute__((__visibility__("hidden")))

// Library constructor
#define _nattr_constructor __attribute__((constructor))

// Noreturn Function
#define _nattr_noreturn __attribute__((noreturn))

#endif
