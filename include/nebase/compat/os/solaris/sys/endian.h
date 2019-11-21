
#ifndef NEB_COMPAT_SYS_ENDIAN_H
#define NEB_COMPAT_SYS_ENDIAN_H 1

#include <sys/byteorder.h>

#define LITTLE_ENDIAN   1234    /* least-significant byte first (vax, pc) */
#define BIG_ENDIAN      4321    /* most-significant byte first (IBM, net) */
#define PDP_ENDIAN      3412    /* LSB first in word, MSW first in long (pdp) */

#if defined(_LITTLE_ENDIAN)
# define BYTE_ORDER LITTLE_ENDIAN

# define htobe16(x) BSWAP_16(x)
# define htole16(x) (x)
# define be16toh(x) BSWAP_16(x)
# define le16toh(x) (x)

# define htobe32(x) BSWAP_32(x)
# define htole32(x) (x)
# define be32toh(x) BSWAP_32(x)
# define le32toh(x) (x)

# define htobe64(x) BSWAP_64(x)
# define htole64(x) (x)
# define be64toh(x) BSWAP_64(x)
# define le64toh(x) (x)
#elif defined(_BIG_ENDIAN)
# define BYTE_ORDER BIG_ENDIAN

# define htobe16(x) (x)
# define htole16(x) BSWAP_16(x)
# define be16toh(x) (x)
# define le16toh(x) BSWAP_16(x)

# define htobe32(x) (x)
# define htole32(x) BSWAP_32(x)
# define be32toh(x) (x)
# define le32toh(x) BSWAP_32(x)

# define htobe64(x) (x)
# define htole64(x) BSWAP_64(x)
# define be64toh(x) (x)
# define le64toh(x) BSWAP_64(x)
#else
# error "Unsupported architecture"
#endif

#endif
