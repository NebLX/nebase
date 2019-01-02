
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/pipe.h>

#include <unistd.h>
#include <fcntl.h>

int neb_pipe_new(int pipefd[2])
{
#if defined(OS_DARWIN)
	if (pipe(pipefd) == -1) {
		neb_syslog(LOG_ERR, "pipe: %m");
		return -1;
	}
	for (int i = 0; i < 2; i++) {
		int fd = pipefd[0];
		if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
			neb_syslog(LOG_ERR, "fcntl(F_SETFL, O_NONBLOCK): %m");
			close(pipefd[0]);
			close(pipefd[1]);
			return -1;
		}
		if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
			neb_syslog(LOG_ERR, "fcntl(F_SETFD, FD_CLOEXEC): %m");
			close(pipefd[0]);
			close(pipefd[1]);
			return -1;
		}
	}
#else
	if (pipe2(pipefd, O_CLOEXEC | O_NONBLOCK) == -1) {
		neb_syslog(LOG_ERR, "pipe2: %m");
		return -1;
	}
#endif
	return 0;
}
