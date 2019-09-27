
#ifndef NEB_COMPAT_SYS_CDEFS_H
#define NEB_COMPAT_SYS_CDEFS_H 1

#include_next <sys/cdefs.h>

#ifndef __attribute_pure__
# define __attribute_pure__ __attribute__((__pure__))
#endif

#endif
