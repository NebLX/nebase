
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/io.h>

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>

#if defined(OS_SOLARIS)
# include <stropts.h>
# include <sys/termios.h>
#else
# include <sys/ioctl.h>
#endif

static const char dev_null[] = "/dev/null";

static int io_dup(int n, int src_fd, ...)
{
	int ret = 0;
	sigset_t old_set;
	sigset_t set;
	sigfillset(&set);

	if (sigprocmask(SIG_SETMASK, &set, &old_set) == -1) {
		neb_syslog(LOG_ERR, "sigprocmask(block all): %m");
		return -1;
	}

	int null_fd = -1;
	if (src_fd < 0) {
		null_fd = open(dev_null, O_RDWR);
		if (null_fd == -1) {
			neb_syslog(LOG_ERR, "open(%s): %m", dev_null);
			return -1;
		}
		src_fd = null_fd;
	}

	va_list va;
	va_start(va, src_fd);

	for (int i = 0; i < n; i++) {
		int dst_fd = va_arg(va, int);
		if (dup2(src_fd, dst_fd) == -1) {
			neb_syslog(LOG_ERR, "dup2(%d -> %d): %m", src_fd, dst_fd);
			ret = -1;
			break;
		}
	}

	va_end(va);

	if (null_fd >= 0)
		close(null_fd);

	sigprocmask(SIG_SETMASK, &old_set, NULL);

	return ret;
}

int neb_io_redirect_stdin(int fd)
{
	return io_dup(1, fd, 0);
}

int neb_io_redirect_stdout(int fd)
{
	return io_dup(1, fd, 1);
}

int neb_io_redirect_stderr(int fd)
{
	return io_dup(1, fd, 2);
}

int neb_io_redirect_pty(int slave_fd)
{
	return io_dup(3, slave_fd, 0, 1, 2);
}

int neb_io_pty_open_master(void)
{
	int fd = posix_openpt(O_RDWR | O_NOCTTY);
	if (fd == -1) {
		neb_syslog(LOG_ERR, "posix_openpt(O_NOCTTY): %m");
		return -1;
	}

	return fd;
}

int neb_io_pty_open_slave(int master_fd)
{
	/*
	 * Use the new CTTY set stype: open(O_NOCTTY) then ioctl(TIOCSCTTY)
	 */

	if (grantpt(master_fd) == -1) {
		neb_syslog(LOG_ERR, "grantpt: %m");
		return -1;
	}
	if (unlockpt(master_fd) == -1) {
		neb_syslog(LOG_ERR, "unlockpt: %m");
		return -1;
	}

	// TODO use sysconf TTY_NAME_MAX
#if defined(OS_LINUX) || defined(OS_DARWIN)
	char name[PATH_MAX];
	if (ptsname_r(master_fd, name, sizeof(name)) != 0) {
		neb_syslog(LOG_ERR, "ptsname_r: %m");
		return -1;
	}
#elif defined(OS_NETBSD)
	char name[PATH_MAX];
	int err = ptsname_r(master_fd, name, sizeof(name));
	if (err != 0) {
		neb_syslog_en(err, LOG_ERR, "ptsname_r: %m");
		return -1;
	}
#elif defined(OS_FREEBSD) || defined(OS_DFLYBSD) || defined(OS_OPENBSD) || defined(OS_SOLARIS)
	char *name = ptsname(master_fd);
	if (!name) {
		neb_syslog(LOG_ERR, "ptsname: %m");
		return -1;
	}
#else
# error "fix me"
#endif

	int fd = open(name, O_RDWR | O_NOCTTY);
	if (fd == -1) {
		neb_syslog(LOG_ERR, "open(%s): %m", name);
		return -1;
	}

	return fd;
}

int neb_io_pty_associate(int slave_fd)
{
#if defined(OS_LINUX)
	int steal = 0;
	if (ioctl(slave_fd, TIOCSCTTY, steal) == -1) {
		neb_syslog(LOG_ERR, "ioctl(TIOCSCTTY): %m");
		return -1;
	}
#elif defined(OSTYPE_BSD) || defined(OS_SOLARIS) || defined(OS_DARWIN)
	if (ioctl(slave_fd, TIOCSCTTY) == -1) {
		neb_syslog(LOG_ERR, "ioctl(TIOCSCTTY): %m");
		return -1;
	}
#else
# error "fix me"
#endif

	return 0;
}

int neb_io_pty_disassociate(int slave_fd)
{
	if (ioctl(slave_fd, TIOCNOTTY) == -1) {
		neb_syslog(LOG_ERR, "ioctl(TIOCNOTTY): %m");
		return -1;
	}
	return 0;
}
