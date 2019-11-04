
#ifndef NEB__INIT_H
#define NEB__INIT_H 1

#include <nebase/cdefs.h>

extern void neb_sock_do_sysconf(void) _nattr_hidden;
extern void neb_pty_do_sysconf(void) _nattr_hidden;

extern void neb_lib_init_sysconf(void) _nattr_constructor;

#endif
