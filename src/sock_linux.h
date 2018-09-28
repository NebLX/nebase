
#ifndef NEB_SOCK_LINUX_H
#define NEB_SOCK_LINUX_H 1

#include <nebase/cdefs.h>
#include <nebase/file.h>

#include <sys/types.h>

extern int neb_sock_unix_get_ino(neb_ino_t *fs_ni, ino_t *sock_ino)
	neb_attr_nonnull((1, 2));

#endif
