
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/sock.h>
#include <nebase/file.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <fcntl.h>

#if defined(OS_LINUX)
# define NEB_SIZE_UCRED sizeof(struct ucred)
# define NEB_SCM_CREDS SCM_CREDENTIALS
# include "sock/linux.h"
#elif defined(OS_FREEBSD)
# define NEB_SIZE_UCRED sizeof(struct cmsgcred)
# define NEB_SCM_CREDS SCM_CREDS
# include "sock/freebsd.h"
#elif defined(OS_DFLYBSD)
# define NEB_SIZE_UCRED sizeof(struct cmsgcred)
# define NEB_SCM_CREDS SCM_CREDS
# include "sock/dflybsd.h"
#elif defined(OS_NETBSD)
# define NEB_SIZE_UCRED SOCKCREDSIZE(0)
# define NEB_SCM_CREDS SCM_CREDS
# include "sock/netbsd.h"
#elif defined(OS_SOLARIS)
// dgram works through SCM
// stream & seqpacket works through getpeerucred()
# include <ucred.h>
# define NEB_SIZE_UCRED ucred_size()
# define NEB_SCM_CREDS SCM_UCRED
# include "sock/solaris.h"
#elif defined(OS_OPENBSD)
//NOTE cred work with listen/connect sockets only, no socketpair support
//  we need to wait for upstream support
# include "sock/openbsd.h"
#elif defined(OS_HAIKU)
//NOTE cred: may be the same support level with openbsd
//NOTE no unix path check
#elif defined(OS_DARWIN)
//NOTE only support stream, no protocol seqpacket, and no support for dgram
# include <sys/ucred.h>
# ifndef MSG_NOSIGNAL
#  define MSG_NOSIGNAL 0
# endif
//NOTE no unix path check, as Apple use private (FreeBSD) headers
#else
# error "fix me"
#endif

int neb_sock_unix_path_in_use(const char *path, int *in_use, int *type)
{
	int ret = 0;
	*in_use = 0;
	*type = 0;
#if defined(OS_LINUX)
	neb_ino_t fs_ni;
	if (neb_file_get_ino(path, &fs_ni) != 0) {
		neb_syslog(LOG_ERR, "Failed to get ino for %s", path);
		return -1;
	}
	ino_t sock_ino = 0;
	if (neb_sock_unix_get_ino(&fs_ni, &sock_ino, type) != 0)
		ret = -1;
	if (sock_ino)
		*in_use = 1;
#elif defined(OS_FREEBSD)
	kvaddr_t sockptr = 0;
	if (neb_sock_unix_get_sockptr(path, &sockptr, type) != 0)
		ret = -1;
	if (sockptr)
		*in_use = 1;
#elif defined(OS_NETBSD)
	uint64_t sockptr = 0;
	if (neb_sock_unix_get_sockptr(path, &sockptr, type) != 0)
		ret = -1;
	if (sockptr)
		*in_use = 1;
#elif defined(OS_DFLYBSD)
	void *sockptr = NULL;
	if (neb_sock_unix_get_sockptr(path, &sockptr, type) != 0)
		ret = -1;
	if (sockptr)
		*in_use = 1;
#elif defined(OS_OPENBSD)
	uint64_t sockptr = 0;
	if (neb_sock_unix_get_sockptr(path, &sockptr, type) != 0)
		ret = -1;
	if (sockptr)
		*in_use = 1;
#elif defined(OS_SOLARIS)
	uint64_t sockptr = 0;
	if (neb_sock_unix_get_sockptr(path, &sockptr, type) != 0)
		ret = -1;
	if (sockptr)
		*in_use = 1;
#else
	neb_syslog(LOG_INFO, "Unix sock path check is not available in this platform");
	ret = 1;
#endif
	if (ret < 0)
		neb_syslog(LOG_ERR, "Failed to check if %s is in use", path);
	return ret;
}

/**
 * \return 0 if not in use
 */
static int sock_unix_addr_in_use(const char *addr)
{
	int in_use = 0, type = 0;
	int ret = neb_sock_unix_path_in_use(addr, &in_use, &type);
	if (ret != 0) {
		neb_syslog(LOG_INFO, "Default to in use");
		in_use = 1;
	}
	return in_use;
}

int neb_sock_unix_new_binded(int type, const char *addr)
{
	if (strlen(addr) > NEB_UNIX_ADDR_MAXLEN) {
		neb_syslog(LOG_ERR, "Invalid unix socket addr %s: length overflow", addr);
		return -1;
	}

	switch (neb_file_get_type(addr)) {
	case NEB_FTYPE_NOENT:
		break;
	case NEB_FTYPE_SOCK:
		if (sock_unix_addr_in_use(addr) == 0) {
			neb_syslog(LOG_INFO, "Unlink previous sock file %s which is not in use", addr);
			unlink(addr);
		} else {
			neb_syslog(LOG_ERR, "File %s exists as a sock file and is in use", addr);
			return -1;
		}
		break;
	case NEB_FTYPE_UNKNOWN:
		neb_syslog(LOG_ERR, "Failed to get type of file %s", addr);
		return -1;
		break;
	default:
		neb_syslog(LOG_ERR, "File %s exists as a non-sock file", addr);
		return -1;
		break;
	}

	int fd = socket(AF_UNIX, type | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (fd == -1) {
		neb_syslog(LOG_ERR, "socket: %m");
		return -1;
	}

	struct sockaddr_un saddr;
	saddr.sun_family = AF_UNIX;
	strncpy(saddr.sun_path, addr, sizeof(saddr.sun_path));

	if (bind(fd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
		neb_syslog(LOG_ERR, "bind(%s): %m", addr);
		close(fd);
		return -1;
	}

	return fd;
}

int neb_sock_unix_new_connected(int type, const char *addr, int timeout)
{
	if (strlen(addr) > NEB_UNIX_ADDR_MAXLEN) {
		neb_syslog(LOG_ERR, "Invalid unix socket addr %s: length overflow", addr);
		return -1;
	}

	int fd = socket(AF_UNIX, type | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (fd == -1) {
		neb_syslog(LOG_ERR, "socket: %m");
		return -1;
	}

	struct sockaddr_un saddr;
	saddr.sun_family = AF_UNIX;
	strncpy(saddr.sun_path, addr, sizeof(saddr.sun_path));

	if (connect(fd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1 && errno != EINPROGRESS) {
		neb_syslog(LOG_ERR, "connect(%s): %m", addr);
		close(fd);
		return -1;
	}

	struct pollfd pfd = {
		.fd = fd,
		.events = POLLOUT
	};
	for (;;) {
		int ret = poll(&pfd, 1, timeout);
		switch (ret) {
		case -1:
			if (errno == EINTR)
				continue;
			neb_syslog(LOG_ERR, "poll: %m");
			close(fd);
			return -1;
			break;
		case 0:
			errno = ETIMEDOUT;
			close(fd);
			return -1;
			break;
		default:
			break;
		}

		int soe = 0;
		socklen_t soe_len = sizeof(soe);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soe, &soe_len) == -1) {
			neb_syslog(LOG_ERR, "getsockopt(SO_ERROR): %m");
			close(fd);
			return -1;
		}
		if (soe != 0) {
			neb_syslog_en(soe, LOG_ERR, "connect(%s): %m", addr);
			close(fd);
			return -1;
		}
		return fd;
	}
}

#if defined(OS_LINUX) || defined(OS_NETBSD) || defined(OS_SOLARIS)
int neb_sock_unix_enable_recv_cred(int fd)
{
# if defined(OS_LINUX)
	int passcred = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &passcred, sizeof(passcred)) == -1) {
		neb_syslog(LOG_ERR, "setsockopt(SO_PASSCRED): %m");
# elif defined(OS_NETBSD)
	int passcred = 1; // local sockopt level is 0, see in src/lib/libc/net/getpeereid.c
	if (setsockopt(fd, 0, LOCAL_CREDS, &passcred, sizeof(passcred)) == -1) {
		neb_syslog(LOG_ERR, "setsockopt(LOCAL_CREDS): %m");
# elif defined(OS_SOLARIS)
	int recvucred = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_RECVUCRED, &recvucred, sizeof(recvucred)) == -1) {
		if (errno == EINVAL) // Will fail for stream & seqpacket
			return 0;
		neb_syslog(LOG_ERR, "setsocketopt(SO_RECVUCRED): %m");
# endif
		return -1;
	}
	return 0;
}
#else
int neb_sock_unix_enable_recv_cred(int fd __attribute_unused__)
{
	return 0;
}
#endif

#if defined(OS_FREEBSD) || defined(OS_DFLYBSD)
int neb_sock_unix_send_with_cred(int fd, const char *data, int len, void *name, socklen_t namelen)
{
	struct iovec iov = {
		.iov_base = (void *)data,
		.iov_len = len
	};
	char buf[CMSG_SPACE(NEB_SIZE_UCRED)];
	struct msghdr msg = {
		.msg_name = name,
		.msg_namelen = namelen,
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = buf,
		.msg_controllen = sizeof(buf)
	};

	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = NEB_SCM_CREDS;
	cmsg->cmsg_len = CMSG_LEN(NEB_SIZE_UCRED);

# if defined(OS_FREEBSD) || defined(OS_DFLYBSD)
	struct cmsgcred *u = (struct cmsgcred *)CMSG_DATA(cmsg);
	memset(u, 0, NEB_SIZE_UCRED);
# endif

	ssize_t nw = sendmsg(fd, &msg, MSG_NOSIGNAL);
	if (nw == -1) {
		neb_syslog(LOG_ERR, "sendmsg: %m");
		return -1;
	}

	return nw;
}
#else
int neb_sock_unix_send_with_cred(int fd, const char *data, int len, void *name, socklen_t namelen)
{
	struct iovec iov = {
		.iov_base = (void *)data,
		.iov_len = len
	};
	struct msghdr msg = {
		.msg_name = name,
		.msg_namelen = namelen,
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = NULL,
		.msg_controllen = 0
	};

	ssize_t nw = sendmsg(fd, &msg, MSG_NOSIGNAL);
	if (nw == -1) {
		neb_syslog(LOG_ERR, "sendmsg: %m");
		return -1;
	}

	return nw;
}
#endif

#if defined(OS_OPENBSD) || defined(OS_DARWIN) || defined(OS_HAIKU)
int neb_sock_unix_recv_with_cred(int fd, char *data, int len, struct neb_ucred *pu)
{
# if defined(OS_OPENBSD) || defined(OS_HAIKU)
#  if defined(OS_OPENBSD)
	struct sockpeercred scred;
#  elif defined(OS_HAIKU)
	struct ucred scred;
#  else
#   error "fix me"
#  endif
	socklen_t scred_len = sizeof(scred);
	if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &scred, &scred_len) == -1) {
		neb_syslog(LOG_ERR, "getsockopt(SO_PEERCRED): %m");
		return -1;
	}
	pu->uid = scred.uid;
	pu->gid = scred.gid;
	pu->pid = scred.pid;
# elif defined(OS_DARWIN)
	struct xucred xucred;
	socklen_t xucred_len = sizeof(xucred);
	if (getsockopt(fd, SOL_LOCAL, LOCAL_PEERCRED, &xucred, &xucred_len) == -1) {
		neb_syslog(LOG_ERR, "getsockopt(LOCAL_PEERCRED): %m");
		return -1;
	}
	if (xucred.cr_version != XUCRED_VERSION) {
		neb_syslog(LOG_ERR, "xucred ABI version mismatch: %u while expect %d", xucred.cr_version, XUCRED_VERSION);
		return -1;
	}
	pu->uid = xucred.cr_uid;
	pu->gid = xucred.cr_gid;
	pid_t pid;
	socklen_t pid_len = sizeof(pid);
	if (getsockopt(fd, SOL_LOCAL, LOCAL_PEERPID, &pid, &pid_len) == -1) {
		neb_syslog(LOG_ERR, "getsockopt(LOCAL_PEERPID): %m");
		return -1;
	}
	pu->pid = pid;
# else
#  error "fix me"
# endif

	ssize_t nr = recv(fd, data, len, MSG_NOSIGNAL | MSG_DONTWAIT);
	if (nr == -1) {
		neb_syslog(LOG_ERR, "recv: %m");
		return -1;
	}

	return nr;
}
#else
int neb_sock_unix_recv_with_cred(int fd, char *data, int len, struct neb_ucred *pu)
{
	struct iovec iov = {
		.iov_base = data,
		.iov_len = len
	};
	char buf[CMSG_SPACE(NEB_SIZE_UCRED)];
	struct msghdr msg = {
		.msg_name = NULL,
		.msg_namelen = 0,
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = buf,
		.msg_controllen = sizeof(buf)
	};

	ssize_t nr = recvmsg(fd, &msg, MSG_NOSIGNAL | MSG_DONTWAIT);
	if (nr == -1) {
		neb_syslog(LOG_ERR, "recvmsg: %m");
		return -1;
	}

	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
	if (!cmsg || cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != NEB_SCM_CREDS) {
# if defined(OS_SOLARIS)
		ucred_t *u = NULL;
		if (getpeerucred(fd, &u) == -1) { // for stream and seqpacket
			neb_syslog(LOG_ERR, "getpeerucred: %m");
			return -1;
		}
		pu->uid = ucred_getruid(u);
		pu->gid = ucred_getrgid(u);
		pu->pid = ucred_getpid(u);
		ucred_free(u);
		return nr;
# else
		neb_syslog(LOG_NOTICE, "No credentials received with fd %d", fd);
		return -1;
# endif
	}

# if defined(OS_LINUX)
	const struct ucred *u = (const struct ucred *)CMSG_DATA(cmsg);
	pu->uid = u->uid;
	pu->gid = u->gid;
	pu->pid = u->pid;
# elif defined(OS_FREEBSD) || defined(OS_DFLYBSD)
	const struct cmsgcred *u = (const struct cmsgcred *)CMSG_DATA(cmsg);
	pu->uid = u->cmcred_uid;
	pu->gid = u->cmcred_gid;
	pu->pid = u->cmcred_pid;
# elif defined(OS_NETBSD)
	const struct sockcred *u = (const struct sockcred *)CMSG_DATA(cmsg);
	pu->uid = u->sc_uid;
	pu->gid = u->sc_gid;
	pu->pid = u->sc_pid;
# elif defined(OS_SOLARIS)
	const ucred_t *u = (const ucred_t *)CMSG_DATA(cmsg);
	pu->uid = ucred_getruid(u);
	pu->gid = ucred_getrgid(u);
	pu->pid = ucred_getpid(u);
# else
#  error "fix me"
# endif

	return nr;
}
#endif

int neb_sock_unix_send_with_fds(int fd, const char *data, int len, int *fds, int fd_num, void *name, socklen_t namelen)
{
	struct iovec iov = {
		.iov_base = (void *)data,
		.iov_len = len
	};
	const size_t payload_len = sizeof(int) * fd_num;
	char buf[CMSG_SPACE(payload_len)];
	struct msghdr msg = {
		.msg_name = name,
		.msg_namelen = namelen,
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = buf,
		.msg_controllen = sizeof(buf)
	};

	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(payload_len);
	memcpy(CMSG_DATA(cmsg), fds, payload_len);

	ssize_t nw = sendmsg(fd, &msg, MSG_NOSIGNAL);
	if (nw == -1) {
		neb_syslog(LOG_ERR, "sendmsg: %m");
		return -1;
	}

	return nw;
}

int neb_sock_unix_recv_with_fds(int fd, char *data, int len, int *fds, int *fd_num)
{
	struct iovec iov = {
		.iov_base = data,
		.iov_len = len
	};
	char buf[CMSG_SPACE(sizeof(int) * *fd_num)];
	struct msghdr msg = {
		.msg_name = NULL,
		.msg_namelen = 0,
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = buf,
		.msg_controllen = sizeof(buf)
	};

	ssize_t nr = recvmsg(fd, &msg, MSG_NOSIGNAL | MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
	if (nr == -1) {
		neb_syslog(LOG_ERR, "recvmsg: %m");
		return -1;
	}

	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
	if (!cmsg || cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
		neb_syslog(LOG_NOTICE, "No rights received with fd %d", fd);
		return -1;
	}
	size_t payload_len = cmsg->cmsg_len - sizeof(struct cmsghdr);
	*fd_num = payload_len / sizeof(int);
	if (*fd_num)
		memcpy(fds, CMSG_DATA(cmsg), payload_len);
#ifndef MSG_CMSG_CLOEXEC
	int err = 0;
	for (int i = 0; i < *fd_num; i++) {
		int set = 1;
		if (fcntl(fds[i], F_SETFD, FD_CLOEXEC, &set) == -1) {
			neb_syslog(LOG_ERR, "fcntl(FD_CLOEXEC): %m");
			err = 1;
			break;
		}
	}
	if (err) {
		for (int i = 0; i < *fd_num; i++)
			close(fds[i]);
		return -1;
	}
#endif
	return nr;
}

int neb_sock_timed_read_ready(int fd, int msec)
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

	if (pfd.revents & POLLHUP) {
		errno = EPIPE;
		return 0;
	}
	return 1;
}

int neb_sock_timed_recv_exact(int fd, void *buf, size_t len, int msec)
{
	if (!neb_sock_timed_read_ready(fd, msec)) {
		neb_syslog(LOG_ERR, "Error while waiting to read on fd %d: %m", fd);
		return -1;
	}

	ssize_t nr = recvfrom(fd, buf, len, MSG_DONTWAIT, NULL, NULL);
	if (nr == -1) {
		neb_syslog(LOG_ERR, "recvfrom: %m");
		return -1;
	}
	if ((size_t)nr != len) {
		neb_syslog(LOG_ERR, "recvfrom: size mismatch, real %ld exp %lu", nr, len);
		return -1;
	}

	return 0;
}
