
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/io.h>

#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <stdarg.h>
#include <paths.h>

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
		null_fd = open(_PATH_DEVNULL, O_RDWR);
		if (null_fd == -1) {
			neb_syslog(LOG_ERR, "open(%s): %m", _PATH_DEVNULL);
			return -1;
		}
		src_fd = null_fd;
	}

	va_list va;
	va_start(va, src_fd);

	for (int i = 0; i < n; i++) {
		int dst_fd = va_arg(va, int);
		if (src_fd == dst_fd)
			continue;
		close(dst_fd);
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
	return io_dup(1, fd, STDIN_FILENO);
}

int neb_io_redirect_stdout(int fd)
{
	return io_dup(1, fd, STDOUT_FILENO);
}

int neb_io_redirect_stderr(int fd)
{
	return io_dup(1, fd, STDERR_FILENO);
}

int neb_io_redirect_pty(int slave_fd)
{
	return io_dup(3, slave_fd, STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO);
}
