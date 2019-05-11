
#ifndef NEB_SOCK_XNU_H
#define NEB_SOCK_XNU_H 1

#include <nebase/cdefs.h>

#include <sys/socketvar.h>

#if !CONFIG_EMBEDDED
typedef u_int64_t so_type_t;
#else
typedef _XSOCKET_PTR(struct socket *) so_type_t;
#endif

/**
 * \param[out] sockptr *sockptr equals xf_data in struct xfile
 * \note see source code for netstat for more info
 */
extern int neb_sock_unix_get_sockptr(const char *path, so_type_t *sockptr, int *type)
	_nattr_hidden _nattr_nonnull((1, 2, 3));

#endif
