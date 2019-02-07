
#ifndef NEB_SOCK_SOLARIS_H
#define NEB_SOCK_SOLARIS_H 1

#include <nebase/cdefs.h>

#include <stdint.h>

/**
 * \note refer to illumos netstat code, but with a different sockinfo struct
 *       use `truss netstat -f unix` to check if kstat is still used
 */
extern int neb_sock_unix_get_sockptr(const char *path, uint64_t *sockptr, int *type)
	__attribute_hidden__ neb_attr_nonnull((1, 2, 3));

#endif
