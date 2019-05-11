
#ifndef NEB_SOCK_OPENBSD_H
#define NEB_SOCK_OPENBSD_H 1

#include <nebase/cdefs.h>

#include <stdint.h>

/**
 * \note see source code for netstat for more info
 */
extern int neb_sock_unix_get_sockptr(const char *path, uint64_t *sockptr, int *type)
	_nattr_hidden _nattr_nonnull((1, 2, 3));

#endif
