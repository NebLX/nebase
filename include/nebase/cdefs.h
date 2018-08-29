
#ifndef NEB_CDEFS_H
#define NEB_CDEFS_H 1

// standard cdefs

#if __has_include("sys/cdefs.h")
# include <sys/cdefs.h>
#endif

#ifndef __attribute_warn_unused_result__
# define __attribute_warn_unused_result__ __attribute__ ((__warn_unused_result__))
#endif

#ifndef __attribute_unused__
# define __attribute_unused__ __attribute__((__unused__))
#endif

#ifndef __attribute_deprecated__
# define __attribute_deprecated__ __attribute__((__deprecated__))
#endif

#ifndef __attribute_hidden__
# define __attribute_hidden__ __attribute__((__visibility__("hidden")))
#endif

#ifndef __sysloglike
# ifndef __syslog_attribute__
#  define __syslog__ __printf__
# endif
# define __sysloglike(fmtarg, firstvararg) \
	__attribute__((__format__ (__syslog__, fmtarg, firstvararg)))
#endif

// our cdefs

#define neb_attr_nonnull(x) __attribute__ ((__nonnull__ x))

#endif
