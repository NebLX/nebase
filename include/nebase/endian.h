
#ifndef NEB_ENDIAN_H
#define NEB_ENDIAN_H 1

#include <endian.h>
#include <stdint.h>

#ifndef __BYTE_ORDER

# ifdef _BYTE_ORDER
#  define __BYTE_ORDER _BYTE_ORDER
#  define __LITTLE_ENDIAN _LITTLE_ENDIAN
#  define __BIG_ENDIAN _BIG_ENDIAN
#  define __PDP_ENDIAN _PDP_ENDIAN
# endif

#endif /* __BYTE_ORDER */

/*
 * Compile time swap
 */

#ifndef ___constant_swab16
# define ___constant_swab16(x) ((uint16_t)(                       \
    (((uint16_t)(x) & (uint16_t)0x00ffU) << 8) |                  \
    (((uint16_t)(x) & (uint16_t)0xff00U) >> 8)))
#endif

#ifndef ___constant_swab32
# define ___constant_swab32(x) ((uint32_t)(                       \
    (((uint32_t)(x) & (uint32_t)0x000000ffUL) << 24) |            \
    (((uint32_t)(x) & (uint32_t)0x0000ff00UL) <<  8) |            \
    (((uint32_t)(x) & (uint32_t)0x00ff0000UL) >>  8) |            \
    (((uint32_t)(x) & (uint32_t)0xff000000UL) >> 24)))
#endif

#ifndef ___constant_swab64
# define ___constant_swab64(x) ((uint64_t)(                       \
    (((uint64_t)(x) & (uint64_t)0x00000000000000ffULL) << 56) |   \
    (((uint64_t)(x) & (uint64_t)0x000000000000ff00ULL) << 40) |   \
    (((uint64_t)(x) & (uint64_t)0x0000000000ff0000ULL) << 24) |   \
    (((uint64_t)(x) & (uint64_t)0x00000000ff000000ULL) <<  8) |   \
    (((uint64_t)(x) & (uint64_t)0x000000ff00000000ULL) >>  8) |   \
    (((uint64_t)(x) & (uint64_t)0x0000ff0000000000ULL) >> 24) |   \
    (((uint64_t)(x) & (uint64_t)0x00ff000000000000ULL) >> 40) |   \
    (((uint64_t)(x) & (uint64_t)0xff00000000000000ULL) >> 56)))
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN
# define cpu_to_le16(x) (x)
# define cpu_to_le32(x) (x)
# define cpu_to_le64(x) (x)
# define cpu_to_be16(x) ___constant_swab16(x)
# define cpu_to_be32(x) ___constant_swab32(x)
# define cpu_to_be64(x) ___constant_swab64(x)
#else
# define cpu_to_le16(x) ___constant_swab16(x)
# define cpu_to_le32(x) ___constant_swab32(x)
# define cpu_to_le64(x) ___constant_swab64(x)
# define cpu_to_be16(x) (x)
# define cpu_to_be32(x) (x)
# define cpu_to_be64(x) (x)
#endif

#endif
