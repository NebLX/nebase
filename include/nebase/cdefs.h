
#ifndef NEB_CDEFS_H
#define NEB_CDEFS_H 1

// standard cdefs

#include <sys/cdefs.h>

#ifndef __attribute_unused__
#define __attribute_unused__ __attribute__((__unused__))
#endif

#ifndef __attribute_deprecated__
# define __attribute_deprecated__ __attribute__((__deprecated__))
#endif

#ifndef __attribute_hidden__
# define __attribute_hidden__ __attribute__((__visibility__("hidden")))
#endif

// our cdefs

#define neb_attr_nonnull(x) __attribute__ ((__nonnull__ x))

#endif
