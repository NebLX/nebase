
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

#ifndef __predict_true
# define __predict_true(exp)     __builtin_expect((exp), 1)
#endif
#ifndef __predict_false
# define __predict_false(exp)    __builtin_expect((exp), 0)
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

// Packed structure
#define _nattr_packed __attribute__((__packed__))

// Library constructor
#define _nattr_constructor __attribute__((constructor))

// Noreturn Function
#define _nattr_noreturn __attribute__((noreturn))

// Pure Functions, change nothing, and can have pointer param
#define _nattr_pure __attribute__((pure))

// Const Functions, change nothing, and no pointer param
#define _nattr_const __attribute__((const))

// Attribute malloc indicates that a function is malloc-like
#define _nattr_malloc __attribute__((malloc))
#ifdef HAVE_C_MACRO_MALLOC_EXTENDED
# define _nattr_dealloc(dealloctor, ptr_index) __attribute__((malloc(dealloctor, ptr_index)))
#else
# define _nattr_dealloc(dealloctor, ptr_index)
#endif

#endif
