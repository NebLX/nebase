
#include "options.h"

#include <nebase/sysconf.h>

#include "_init.h"
#include "sock/_init.h"

#include <unistd.h>
#include <limits.h>

int neb_sysconf_pagesize = 512;

#ifdef TTY_NAME_MAX
int neb_sysconf_ttyname_max = TTY_NAME_MAX;
#else
int neb_sysconf_ttyname_max = _POSIX_TTY_NAME_MAX;
#endif

void neb_lib_init_sysconf(void)
{
	neb_sock_unix_do_sysconf();
	neb_sysconf_pagesize = sysconf(_SC_PAGESIZE);
	neb_sysconf_ttyname_max = sysconf(_SC_TTY_NAME_MAX);
}
