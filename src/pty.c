
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/pty.h>

#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>

#if defined(OS_LINUX)
# include <pty.h>
# include <utmp.h>
#elif defined(OS_FREEBSD) || defined(OS_DFLYBSD)
# include <libutil.h>
#elif defined(OS_NETBSD) || defined(OS_OPENBSD)
# include <util.h>
#elif defined(OS_SOLARIS)
// nothing
#else
# error "fix me"
#endif

int neb_pty_openpty(int *amaster, int *aslave, char *name,
                    struct termios *termp, struct neb_pty_winsize *winp)
{
	struct winsize win, *real_winp = NULL;
	if (winp) {
		win.ws_col = winp->ws_col;
		win.ws_row = winp->ws_row;
		win.ws_xpixel = winp->ws_xpixel;
		win.ws_ypixel = winp->ws_ypixel;
		real_winp = &win;
	}

#if defined(OS_LINUX) || defined(OSTYPE_BSD) || defined(OS_SOLARIS)
	if (openpty(amaster, aslave, name, termp, real_winp) == -1) {
		neb_syslog(LOG_ERR, "openpty: %m");
		return -1;
	}
#else
# error "fix me"
#endif

	return 0;
}

int neb_pty_login_tty(int slave_fd)
{
#if defined(OS_LINUX) || defined(OSTYPE_BSD) || defined(OS_SOLARIS)
	if (login_tty(slave_fd) == -1) {
		neb_syslog(LOG_ERR, "login_tty: %m");
		return -1;
	}
#else
# error "fix me"
#endif
	return 0;
}
