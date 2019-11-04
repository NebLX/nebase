
#include "options.h"

#include "_init.h"

void neb_lib_init_sysconf(void)
{
	neb_sock_do_sysconf();
	neb_pty_do_sysconf();
}
