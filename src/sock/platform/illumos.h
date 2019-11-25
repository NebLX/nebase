
#ifndef NEB_SRC_SOCK_PLATFORM_ILLUMOS_H
#define NEB_SRC_SOCK_PLATFORM_ILLUMOS_H 1

#include <nebase/cdefs.h>

#include <stdint.h>

/**
 * \note see illumos netstat code
 */
extern int neb_sock_unix_get_sockptr(const char *path, uint64_t *sockptr, int *type)
	_nattr_hidden _nattr_nonnull((1, 2, 3));

#endif
