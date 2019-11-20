
#ifndef NEB_ENDIAN_H
#define NEB_ENDIAN_H 1

#include <endian.h>

#ifndef __BYTE_ORDER

# ifdef _BYTE_ORDER
#  define __BYTE_ORDER _BYTE_ORDER
#  define __LITTLE_ENDIAN _LITTLE_ENDIAN
#  define __BIG_ENDIAN _BIG_ENDIAN
#  define __PDP_ENDIAN _PDP_ENDIAN
# endif

#endif /* __BYTE_ORDER */

#endif
