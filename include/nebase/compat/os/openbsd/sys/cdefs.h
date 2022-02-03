
#ifndef NEB_COMPAT_SYS_CDEFS_H
#define NEB_COMPAT_SYS_CDEFS_H 1

#include_next <sys/cdefs.h>

#ifndef __sysloglike
# define __sysloglike(fmtarg, firstvararg) \
	__attribute__((__format__ (__syslog__, fmtarg, firstvararg)))
#endif

#ifndef _GL_ATTRIBUTE_PURE
# define _GL_ATTRIBUTE_PURE __attribute__((__pure__))
#endif

#ifndef _GL_ATTRIBUTE_MALLOC
# define _GL_ATTRIBUTE_MALLOC __attribute__((__malloc__))
#endif

#ifndef _GL_ATTRIBUTE_DEALLOC
# ifdef HAVE_C_MACRO_MALLOC_EXTENDED
#  define _GL_ATTRIBUTE_DEALLOC(deallocator, ptr_index) __attribute__((__malloc__(deallocator, ptr_index)))
# else
#  define _GL_ATTRIBUTE_DEALLOC(deallocator, ptr_index)
# endif
#endif

#ifndef _GL_ATTRIBUTE_NODISCARD
# define _GL_ATTRIBUTE_NODISCARD __attribute__((__warn_unused_result__))
#endif

#ifndef _GL_ATTRIBUTE_DEPRECATED
# define _GL_ATTRIBUTE_DEPRECATED __attribute__((__deprecated__))
#endif

#ifndef _GL_ATTRIBUTE_RETURNS_NONNULL
# define _GL_ATTRIBUTE_RETURNS_NONNULL __attribute__((__returns_nonnull__))
#endif

#endif
