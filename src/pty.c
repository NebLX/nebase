
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/pty.h>
#include <nebase/io.h>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <limits.h>
#include <paths.h>

#if defined(OS_LINUX)
# include <pty.h>
# include <utmp.h>
#elif defined(OS_FREEBSD) || defined(OS_DFLYBSD)
# include <libutil.h>
#elif defined(OS_NETBSD) || defined(OS_OPENBSD) || defined(OS_DARWIN)
# include <util.h>
#elif defined(OSTYPE_SUN)
# include <stropts.h>
#else
# error "fix me"
#endif

#ifndef TTY_NAME_MAX
# define TTY_NAME_MAX _POSIX_TTY_NAME_MAX
#endif

int neb_pty_ptsname(int master_fd, char *buf, size_t buflen)
{
#if defined(OS_LINUX) || defined(OS_DARWIN)
	if (ptsname_r(master_fd, buf, buflen) != 0) {
		neb_syslog(LOG_ERR, "ptsname_r: %m");
		return -1;
	}
#elif defined(OS_NETBSD)
	int err = ptsname_r(master_fd, buf, buflen);
	if (err != 0) {
		neb_syslog_en(err, LOG_ERR, "ptsname_r: %m");
		return -1;
	}
#elif defined(OS_FREEBSD) || defined(OS_DFLYBSD) || defined(OS_OPENBSD) || defined(OSTYPE_SUN)
	char *slave = ptsname(master_fd);
	if (!slave) {
		neb_syslog(LOG_ERR, "ptsname: %m");
		return -1;
	}
	buf[buflen - 1] = '\0';
	strncpy(buf, slave, buflen-1);
#else
# error "fix me"
#endif
	return 0;
}

int neb_pty_ttyname(int slave_fd, char *buf, size_t buflen)
{
	return ttyname_r(slave_fd, buf, buflen);
}

int neb_pty_change_winsz(int master_fd, const struct neb_pty_winsize *winp)
{
	struct winsize w = {
		.ws_row = winp->ws_row,
		.ws_col = winp->ws_col,
		.ws_xpixel = winp->ws_xpixel,
		.ws_ypixel = winp->ws_ypixel,
	};

	if (ioctl(master_fd, TIOCSWINSZ, &w) == -1) {
		neb_syslog(LOG_ERR, "ioctl(TIOCSWINSZ): %m");
		return -1;
	}
	return 0;
}

int neb_pty_openpty(int *amaster, int *aslave)
{
#if defined(OS_LINUX) || defined(OSTYPE_BSD) || defined(OS_SOLARIS) || defined(OS_DARWIN)
	if (openpty(amaster, aslave, NULL, NULL, NULL) == -1) {
		neb_syslog(LOG_ERR, "openpty: %m");
		return -1;
	}
#else
	*amaster = posix_openpt(O_RDWR | O_NOCTTY);
	if (*amaster == -1) {
		neb_syslog(LOG_ERR, "posix_openpt: %m");
		return -1;
	}

	struct sigaction old, new;
	new.sa_handler = SIG_DFL;
	new.sa_flags = 0;
	sigemptyset(&new.sa_mask);
	if (sigaction(SIGCHLD, &new, &old) == -1) {
		neb_syslog(LOG_ERR, "sigaction: %m");
		close(*amaster);
		return -1;
	}

	if (grantpt(*amaster) == -1) {
		neb_syslog(LOG_ERR, "grantpt: %m");
		close(*amaster);
		return -1;
	}

	if (sigaction(SIGCHLD, &old, NULL) == -1) {
		neb_syslog(LOG_ERR, "sigaction: %m");
		close(*amaster);
		return -1;
	}

	if (unlockpt(*amaster) == -1) {
		neb_syslog(LOG_ERR, "unlockpt: %m");
		close(*amaster);
		return -1;
	}

# ifdef TIOCGPTPEER
	// at least for Linux >= 4.13, see ioctl_tty(2)
	*aslave = ioctl(*amaster, TIOCGPTPEER, O_RDWR | O_NOCTTY);
	if (*aslave == -1) {
		neb_syslog(LOG_ERR, "ioctl(TIOCGPTPEER): %m");
		close(*amaster);
		return -1;
	}
# else
	// the traditioanal way, open() ptsname()
	char slave[TTY_NAME_MAX];
	if (neb_pty_ptsname(*amaster, slave, sizeof(slave)) != 0) {
		close(*amaster);
		return -1;
	}

	*aslave = open(slave, O_RDWR | O_NOCTTY);
	if (*aslave == -1) {
		neb_syslog(LOG_ERR, "open(%s): %m", slave);
		close(*amaster);
		return -1;
	}
# endif

# ifdef OSTYPE_SUN
	int pushed = ioctl(*aslave, I_FIND, "ldterm");
	if (pushed == -1) {
		neb_syslog(LOG_ERR, "ioctl(I_FIND/ldterm): %m");
		close(*amaster);
		close(*aslave);
		return -1;
	}
	if (!pushed) {
		if (ioctl(*aslave, I_PUSH, "ptem") == -1) {
			neb_syslog(LOG_ERR, "ioctl(I_PUSH/ptem): %m");
			close(*amaster);
			close(*aslave);
			return -1;
		}
		if (ioctl(*aslave, I_PUSH, "ldterm") == -1) {
			neb_syslog(LOG_ERR, "ioctl(I_PUSH/ldterm): %m");
			close(*amaster);
			close(*aslave);
			return -1;
		}
	}
# endif
#endif

	return 0;
}

int neb_pty_login_tty(int slave_fd)
{
#if defined(OS_LINUX) || defined(OSTYPE_BSD) || defined(OS_SOLARIS) || defined(OS_DARWIN)
	if (login_tty(slave_fd) == -1) {
		neb_syslog(LOG_ERR, "login_tty: %m");
		return -1;
	}
#elif defined(OS_ILLUMOS)
	if (neb_pty_make_ctty(slave_fd) != 0)
		return -1;
	if (neb_io_redirect_pty(slave_fd) != 0)
		return -1;
	if (slave_fd > STDERR_FILENO)
		close(slave_fd);
#else
# error "fix me"
#endif

	return 0;
}

int neb_pty_make_ctty(int slave_fd)
{
	neb_pty_disconnect_ctty();

	if (setsid() == -1) {
		neb_syslog(LOG_ERR, "setsid: %m");
		return -1;
	}

	/*
	 * Verify that we are successfully disconnected from the controlling
	 * tty.
	 */
	int fd = open(_PATH_TTY, O_RDWR | O_NOCTTY);
	if (fd >= 0) {
		neb_syslog(LOG_ERR, "Failed to disconnect from controlling tty");
		close(fd);
		return -1;
	}

#ifdef TIOCSCTTY
# if defined(OS_LINUX)
	int steal = 0;
	if (ioctl(slave_fd, TIOCSCTTY, steal) == -1)
		neb_syslog(LOG_ERR, "ioctl(TIOCSCTTY): %m");
# elif defined(OSTYPE_BSD) || defined(OSTYPE_SUN) || defined(OS_DARWIN)
	if (ioctl(slave_fd, TIOCSCTTY, NULL) == -1)
		neb_syslog(LOG_ERR, "ioctl(TIOCSCTTY): %m");
# else
#  error "fix me"
# endif
#endif

	char tty[TTY_NAME_MAX];
	if (neb_pty_ttyname(slave_fd, tty, sizeof(tty)) != 0) {
		neb_syslog(LOG_ERR, "Failed to get ttyname");
		return -1;
	}

	fd = open(tty, O_RDWR); // NOTE no O_NOCTTY here
	if (fd < 0) {
		neb_syslog(LOG_ERR, "open(%s): %m", tty);
	} else {
		close(fd);
	}

	/* Verify that we now have a controlling tty. */
	fd = open(_PATH_TTY, O_WRONLY);
	if (fd < 0) {
		neb_syslog(LOG_ERR, "open(%s) failed - could not set controlling tty: %m", _PATH_TTY);
		return -1;
	} else {
		close(fd);
		return 0;
	}
}

void neb_pty_disconnect_ctty(void)
{
	int fd;

	if ((fd = open(_PATH_TTY, O_RDWR | O_NOCTTY)) >= 0) {
		(void) ioctl(fd, TIOCNOTTY, NULL);
		close(fd);
	}
}
