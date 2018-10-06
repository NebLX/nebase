
#ifndef NEB_SOCK_NETBSD_H
#define NEB_SOCK_NETBSD_H 1

#include <nebase/cdefs.h>

#include <stdint.h>

/**
 * \param[out] sockptr *sockptr equals ki_fdata in struct kinfo_file
 * \note see source code for sockstat for more info
 */
extern int neb_sock_unix_get_sockptr(const char *path, uint64_t *sockptr)
	neb_attr_nonnull((1, 2));

#endif
