
#ifndef NEB_ENDIAN_H
#define NEB_ENDIAN_H 1

#include <endian.h>
#include <stdint.h>

#ifndef BYTE_ORDER
# error "BYTE_ORDER is not defined"
#endif

/*
 * Compile time swap
 */

#define neb_constant_swab16(x) ((uint16_t)(                       \
    (((uint16_t)(x) & (uint16_t)0x00ffU) << 8) |                  \
    (((uint16_t)(x) & (uint16_t)0xff00U) >> 8)))

#define neb_constant_swab32(x) ((uint32_t)(                       \
    (((uint32_t)(x) & (uint32_t)0x000000ffUL) << 24) |            \
    (((uint32_t)(x) & (uint32_t)0x0000ff00UL) <<  8) |            \
    (((uint32_t)(x) & (uint32_t)0x00ff0000UL) >>  8) |            \
    (((uint32_t)(x) & (uint32_t)0xff000000UL) >> 24)))

#define neb_constant_swab64(x) ((uint64_t)(                       \
    (((uint64_t)(x) & (uint64_t)0x00000000000000ffULL) << 56) |   \
    (((uint64_t)(x) & (uint64_t)0x000000000000ff00ULL) << 40) |   \
    (((uint64_t)(x) & (uint64_t)0x0000000000ff0000ULL) << 24) |   \
    (((uint64_t)(x) & (uint64_t)0x00000000ff000000ULL) <<  8) |   \
    (((uint64_t)(x) & (uint64_t)0x000000ff00000000ULL) >>  8) |   \
    (((uint64_t)(x) & (uint64_t)0x0000ff0000000000ULL) >> 24) |   \
    (((uint64_t)(x) & (uint64_t)0x00ff000000000000ULL) >> 40) |   \
    (((uint64_t)(x) & (uint64_t)0xff00000000000000ULL) >> 56)))

#if BYTE_ORDER == LITTLE_ENDIAN
# define neb_constant_htole16(x) (x)
# define neb_constant_htobe16(x) neb_constant_swab16(x)
# define neb_constant_le16toh(x) (x)
# define neb_constant_be16toh(x) neb_constant_swab16(x)

# define neb_constant_htole32(x) (x)
# define neb_constant_htobe32(x) neb_constant_swab32(x)
# define neb_constant_le32toh(x) (x)
# define neb_constant_be32toh(x) neb_constant_swab32(x)

# define neb_constant_htole64(x) (x)
# define neb_constant_htobe64(x) neb_constant_swab64(x)
# define neb_constant_le64toh(x) (x)
# define neb_constant_be64toh(x) neb_constant_swab64(x)
#else
# define neb_constant_htole16(x) neb_constant_swab16(x)
# define neb_constant_htobe16(x) (x)
# define neb_constant_le16toh(x) neb_constant_swab16(x)
# define neb_constant_be16toh(x) (x)

# define neb_constant_htole32(x) neb_constant_swab32(x)
# define neb_constant_htobe32(x) (x)
# define neb_constant_le32toh(x) neb_constant_swab32(x)
# define neb_constant_be32toh(x) (x)

# define neb_constant_htole64(x) neb_constant_swab64(x)
# define neb_constant_htobe64(x) (x)
# define neb_constant_le64toh(x) neb_constant_swab64(x)
# define neb_constant_be64toh(x) (x)
#endif

#endif
