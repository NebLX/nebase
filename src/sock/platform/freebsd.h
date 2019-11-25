
#ifndef NEB_SRC_SOCK_PLATFORM_FREEBSD_H
#define NEB_SRC_SOCK_PLATFORM_FREEBSD_H 1

#include <nebase/cdefs.h>

#include <kvm.h>

/**
 * \param[out] sockptr *sockptr equals xf_data in struct xfile
 * \note see source code for sockstat for more info
 */
extern int neb_sock_unix_get_sockptr(const char *path, kvaddr_t *sockptr, int *type)
	_nattr_hidden _nattr_nonnull((1, 2, 3));

#endif
