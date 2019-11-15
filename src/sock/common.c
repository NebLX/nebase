
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/sock/common.h>

#include <poll.h>
#include <errno.h>

int neb_sock_timed_read_ready(int fd, int msec, int *hup)
{
	struct pollfd pfd = {
		.fd = fd,
		.events = POLLIN,
	};
	for (;;) {
		switch (poll(&pfd, 1, msec)) {
		case -1:
			if (errno == EINTR)
				continue;
			neb_syslog(LOG_ERR, "poll: %m");
			errno = 0;
			return 0;
			break;
		case 0:
			errno = ETIMEDOUT;
			return 0;
			break;
		default:
			break;
		}

		break;
	}

	if (pfd.revents & POLLHUP)
		*hup = 1;
	else
		*hup = 0;
	return pfd.revents & POLLIN;
}

int neb_sock_check_peer_closed(int fd, int msec, neb_sock_check_eof_t is_eof, void *udata)
{
	struct pollfd pfd = {
		.fd = fd,
#if defined(OS_LINUX)
		.events = POLLIN | POLLRDHUP,
#else
		.events = POLLIN,
#endif
	};

	for (;;) {
		switch (poll(&pfd, 1, msec)) {
		case -1:
			if (errno == EINTR)
				continue;
			neb_syslog(LOG_ERR, "poll: %m");
			errno = 0;
			return 0;
			break;
		case 0:
			errno = ETIMEDOUT;
			return 0;
			break;
		default:
			if (pfd.revents & POLLHUP)
				return 1;
			if (pfd.revents & POLLIN) {
				if (!is_eof)
					return 1;
				if (is_eof(fd, pfd.revents, udata))
					return 1;
			}
			break;
		}

		return 0;
	}
}

int neb_sock_recv_exact(int fd, void *buf, size_t len)
{
	ssize_t nr = recv(fd, buf, len, MSG_DONTWAIT);
	if (nr == -1) {
		neb_syslog(LOG_ERR, "recv: %m");
		return -1;
	}
	if ((size_t)nr != len) {
		neb_syslog(LOG_ERR, "recv: size mismatch, real %zd exp %zu", nr, len);
		return -1;
	}

	return 0;
}

int neb_sock_send_exact(int fd, const void *buf, size_t len)
{
	ssize_t nw = send(fd, buf, len, MSG_DONTWAIT);
	if (nw == -1) {
		neb_syslog(LOG_ERR, "send: %m");
		return -1;
	}
	if ((size_t)nw != len) {
		neb_syslog(LOG_ERR, "send: size mismatch, real %zd exp %zu", nw, len);
		return -1;
	}

	return 0;
}
