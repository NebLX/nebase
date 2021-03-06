
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/sock/common.h>
#include <nebase/sock/unix.h>
#include <nebase/sock/inet.h>
#include <nebase/file/stat.h>

#include "_init.h"

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
# include "platform/linux.h"
#elif defined(OS_FREEBSD)
# define NEB_SCM_CREDS SCM_CREDS2
# include "platform/freebsd.h"
#elif defined(OS_NETBSD)
# define NEB_SCM_CREDS SCM_CREDS
# include "platform/netbsd.h"
#elif defined(OS_DFLYBSD)
// DragonFly BSD 6.0 support SO_PASSCRED, but there's no SCM_* flag for ucred
# define NEB_SIZE_UCRED sizeof(struct cmsgcred)
# define NEB_SCM_CREDS SCM_CREDS
# include "platform/dflybsd.h"
#elif defined(OS_SOLARIS)
// dgram works through SCM
// stream & seqpacket works through getpeerucred()
# include <ucred.h>
//# define NEB_SIZE_UCRED ucred_size()
# define NEB_SCM_CREDS SCM_UCRED
# include "platform/solaris.h"
#elif defined(OS_ILLUMOS)
// dgram works through SCM
// stream & seqpacket works through getpeerucred()
# include <ucred.h>
//# define NEB_SIZE_UCRED ucred_size()
# define NEB_SCM_CREDS SCM_UCRED
# include "platform/illumos.h"
#elif defined(OS_OPENBSD)
//NOTE cred work with listen/connect sockets only, no socketpair support
//  we need to wait for upstream support
# include "platform/openbsd.h"
#elif defined(OS_HAIKU)
//NOTE cred: may be the same support level with openbsd
//NOTE no unix path check
#elif defined(OS_DARWIN)
//NOTE only support stream, no protocol seqpacket, and no support for dgram
# include <sys/ucred.h>
# include "platform/xnu.h"
#else
# error "fix me"
#endif

#ifdef NEB_SIZE_UCRED
size_t neb_sock_ucred_cmsg_size = NEB_SIZE_UCRED;
#else
size_t neb_sock_ucred_cmsg_size = 0;
#endif

void neb_sock_unix_do_sysconf(void)
{
#if defined(OSTYPE_SUN)
	neb_sock_ucred_cmsg_size = ucred_size();
#elif defined(OS_FREEBSD)
	neb_sock_ucred_cmsg_size = SOCKCRED2SIZE(sysconf(_SC_NGROUPS_MAX));
#elif defined(OS_NETBSD)
	/* use `getconf NGROUPS_MAX` to verify the final value */
	neb_sock_ucred_cmsg_size = SOCKCREDSIZE(sysconf(_SC_NGROUPS_MAX));
#endif
}

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
#elif defined(OSTYPE_SUN)
	uint64_t sockptr = 0;
	if (neb_sock_unix_get_sockptr(path, &sockptr, type) != 0)
		ret = -1;
	if (sockptr)
		*in_use = 1;
#elif defined(OS_DARWIN)
	so_type_t sockptr = (so_type_t)0;
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

int neb_sock_unix_new(int type)
{
#ifdef SOCK_NONBLOCK
	type |= SOCK_NONBLOCK;
#endif
#ifdef SOCK_CLOEXEC
	type |= SOCK_CLOEXEC;
#endif
	int fd = socket(AF_UNIX, type, 0);
	if (fd == -1) {
		neb_syslogl(LOG_ERR, "socket: %m");
		return -1;
	}
#ifndef SOCK_NONBLOCK
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
		neb_syslogl(LOG_ERR, "fcntl(F_SETFL, O_NONBLOCK): %m");
		close(fd);
		return -1;
	}
#endif
#ifndef SOCK_CLOEXEC
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
		neb_syslogl(LOG_ERR, "fcntl(F_SETFD, FD_CLOEXEC): %m");
		close(fd);
		return -1;
	}
#endif
	return fd;
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
		neb_syslog(LOG_ERR, "File %s exists as a sock file", addr);
		return -1;
		break;
	case NEB_FTYPE_UNKNOWN:
		neb_syslog(LOG_ERR, "File %s exists with an unknown file type", addr);
		return -1;
		break;
	default:
		neb_syslog(LOG_ERR, "File %s exists as a non-sock file", addr);
		return -1;
		break;
	}

	int fd = neb_sock_unix_new(type);
	if (fd == -1)
		return -1;

	struct sockaddr_un saddr;
	saddr.sun_family = AF_UNIX;
	strncpy(saddr.sun_path, addr, sizeof(saddr.sun_path) - 1);
	saddr.sun_path[sizeof(saddr.sun_path) - 1] = '\0';

	if (bind(fd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
		neb_syslogl(LOG_ERR, "bind(%s): %m", addr);
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

	int fd = neb_sock_unix_new(type);
	if (fd == -1)
		return -1;

	struct sockaddr_un saddr;
	saddr.sun_family = AF_UNIX;
	strncpy(saddr.sun_path, addr, sizeof(saddr.sun_path) - 1);
	saddr.sun_path[sizeof(saddr.sun_path) - 1] = '\0';

	if (connect(fd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1 && errno != EINPROGRESS) {
		neb_syslogl(LOG_ERR, "connect(%s): %m", addr);
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
			neb_syslogl(LOG_ERR, "poll: %m");
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
			neb_syslogl(LOG_ERR, "getsockopt(SO_ERROR): %m");
			close(fd);
			return -1;
		}
		if (soe != 0) {
			neb_syslogl_en(soe, LOG_ERR, "connect(%s): %m", addr);
			close(fd);
			return -1;
		}
		return fd;
	}
}

#if defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_NETBSD) || defined(OSTYPE_SUN)
int neb_sock_unix_set_recv_cred(int type, int fd, int enabled)
{
# if defined(OS_LINUX)
	int passcred = enabled;
	if (setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &passcred, sizeof(passcred)) == -1) {
		neb_syslog(LOG_ERR, "setsockopt(SO_PASSCRED, fd: %d, type: %d): %m", fd, type);
# elif defined(OS_FREEBSD)
	int passcred = enabled;
	if (setsockopt(fd, 0, LOCAL_CREDS_PERSISTENT, &passcred, sizeof(passcred)) == -1) {
		neb_syslog(LOG_ERR, "setsockopt(LOCAL_CREDS_PERSISTENT, fd: %d, type: %d): %m", fd, type);
# elif defined(OS_NETBSD)
	int passcred = enabled; // local sockopt level is 0, see in src/lib/libc/net/getpeereid.c
	if (setsockopt(fd, 0, LOCAL_CREDS, &passcred, sizeof(passcred)) == -1) {
		neb_syslog(LOG_ERR, "setsockopt(LOCAL_CREDS, fd: %d, type: %d): %m", fd, type);
# elif defined(OSTYPE_SUN)
	if (type != SOCK_DGRAM)
		return 0;
	int recvucred = enabled;
	if (setsockopt(fd, SOL_SOCKET, SO_RECVUCRED, &recvucred, sizeof(recvucred)) == -1) {
		neb_syslog(LOG_ERR, "setsocketopt(SO_RECVUCRED, fd: %d, type: %d): %m", fd, type);
# endif
		return -1;
	}
	return 0;
}

int neb_sock_unix_enable_recv_cred(int type, int fd)
{
	return neb_sock_unix_set_recv_cred(type, fd ,1);
}

int neb_sock_unix_disable_recv_cred(int type, int fd)
{
	return neb_sock_unix_set_recv_cred(type, fd, 0);
}
#else
int neb_sock_unix_enable_recv_cred(int type _nattr_unused, int fd _nattr_unused)
{
	return 0;
}

int neb_sock_unix_disable_recv_cred(int type _nattr_unused, int fd _nattr_unused)
{
	return 0;
}
#endif

#if defined(OS_DFLYBSD)
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

	struct cmsgcred *u = (struct cmsgcred *)CMSG_DATA(cmsg);
	memset(u, 0, NEB_SIZE_UCRED);

	ssize_t nw = sendmsg(fd, &msg, MSG_NOSIGNAL);
	if (nw == -1) {
		neb_syslogl(LOG_ERR, "sendmsg: %m");
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
		neb_syslogl(LOG_ERR, "sendmsg: %m");
		return -1;
	}

	return nw;
}
#endif

#if defined(OS_OPENBSD) || defined(OS_DARWIN) || defined(OS_HAIKU)
int neb_sock_unix_recv_with_cred(int type, int fd, char *data, int len, struct neb_ucred *pu)
{
	if (type == SOCK_DGRAM) {
		neb_syslog(LOG_ERR, "recv ucred for SOCK_DGRAM sockets is not supported on this platform");
		return -1;
	}

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
		neb_syslogl(LOG_ERR, "recv: %m");
		return -1;
	}

	return nr;
}
#else
int neb_sock_unix_recv_with_cred(int type, int fd, char *data, int len, struct neb_ucred *pu)
{
	struct iovec iov = {
		.iov_base = data,
		.iov_len = len
	};
#if defined(OSTYPE_SUN) || defined(OS_FREEBSD) || defined(OS_NETBSD)
	char buf[neb_sock_ucred_cmsg_size];
#else
	char buf[CMSG_SPACE(NEB_SIZE_UCRED)];
#endif
	struct msghdr msg = {
		.msg_name = NULL,
		.msg_namelen = 0,
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = buf,
		.msg_controllen = sizeof(buf)
	};

	ssize_t nr = recvmsg(fd, &msg, MSG_NOSIGNAL | MSG_DONTWAIT);
	switch (nr) {
	case -1:
		neb_syslogl(LOG_ERR, "recvmsg(fd: %d, type: %d): %m", fd, type); /* fall through */
	case 0:
		return nr;
	default:
		break;
	}

	if (msg.msg_flags & MSG_CTRUNC) {
		neb_syslog(LOG_CRIT, "recvmsg(fd: %d, type: %d): cmsg has ctrunc flag set", fd, type);
		return -1;
	}

#if defined(OSTYPE_SUN)
	if (type != SOCK_DGRAM) { // LATER always check new version of SunOS
		ucred_t *u = NULL;
		if (getpeerucred(fd, &u) == -1) { // for stream and seqpacket
			neb_syslog(LOG_ERR, "getpeerucred: %m");
			return -1;
		}
		pu->uid = ucred_getruid(u);
		pu->gid = ucred_getrgid(u);
		pu->pid = ucred_getpid(u);
		ucred_free(u);
	} else {
		struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
		if (!cmsg || cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != NEB_SCM_CREDS) {
			neb_syslog(LOG_NOTICE, "No credentials received with fd %d", fd);
			return -1;
		}

		const ucred_t *u = (const ucred_t *)CMSG_DATA(cmsg);
		pu->uid = ucred_getruid(u);
		pu->gid = ucred_getrgid(u);
		pu->pid = ucred_getpid(u);

		int recvucred = 0;
		if (setsockopt(fd, SOL_SOCKET, SO_RECVUCRED, &recvucred, sizeof(recvucred)) == -1)
			neb_syslog(LOG_ERR, "setsocketopt(SO_RECVUCRED): %m");
	}

	return nr;
#else
	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
	if (!cmsg || cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != NEB_SCM_CREDS) {
		neb_syslog(LOG_NOTICE, "No credentials received with fd %d", fd);
		return -1;
	}

# if defined(OS_LINUX)
	const struct ucred *u = (const struct ucred *)CMSG_DATA(cmsg);
	pu->uid = u->uid;
	pu->gid = u->gid;
	pu->pid = u->pid;

	int passcred = 0;
	if (setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &passcred, sizeof(passcred)) == -1)
		neb_syslog(LOG_ERR, "setsockopt(SO_PASSCRED): %m");
# elif defined(OS_FREEBSD)
	const struct sockcred2 *u = (const struct sockcred2 *)CMSG_DATA(cmsg);
	pu->uid = u->sc_uid;
	pu->gid = u->sc_gid;
	pu->pid = u->sc_pid;

	int passcred = 0;
	if (setsockopt(fd, 0, LOCAL_CREDS_PERSISTENT, &passcred, sizeof(passcred)) == -1)
		neb_syslog(LOG_ERR, "setsockopt(LOCAL_CREDS_PERSISTENT): %m");
# elif defined(OS_NETBSD)
	const struct sockcred *u = (const struct sockcred *)CMSG_DATA(cmsg);
	pu->uid = u->sc_uid;
	pu->gid = u->sc_gid;
	pu->pid = u->sc_pid;

	if (type == SOCK_DGRAM) {
		int passcred = 0; // local sockopt level is 0, see in src/lib/libc/net/getpeereid.c
		if (setsockopt(fd, 0, LOCAL_CREDS, &passcred, sizeof(passcred)) == -1)
			neb_syslog(LOG_ERR, "setsockopt(LOCAL_CREDS): %m");
	}
# elif defined(OS_DFLYBSD)
	const struct cmsgcred *u = (const struct cmsgcred *)CMSG_DATA(cmsg);
	pu->uid = u->cmcred_uid;
	pu->gid = u->cmcred_gid;
	pu->pid = u->cmcred_pid;
# else
#  error "fix me"
# endif

	return nr;
#endif
}
#endif

int neb_sock_unix_send_with_fds(int fd, const char *data, int len, int *fds, int fd_num, void *name, socklen_t namelen)
{
	struct iovec iov = {
		.iov_base = (void *)data,
		.iov_len = len
	};
	if (fd_num >= NEB_UNIX_MAX_CMSG_FD) {
		neb_syslog(LOG_CRIT, "Sending cmsg fd num %d is not allowed", fd_num);
		return -1;
	}
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
		neb_syslogl(LOG_ERR, "sendmsg: %m");
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
	if (*fd_num >= NEB_UNIX_MAX_CMSG_FD) {
		neb_syslog(LOG_CRIT, "Receiving cmsg fd num %d is not allowed", *fd_num);
		return -1;
	}

	size_t payload_len = sizeof(int) * (*fd_num + 1); // add some more space
	char buf[CMSG_SPACE(payload_len)];
	struct msghdr msg = {
		.msg_name = NULL,
		.msg_namelen = 0,
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = buf,
		.msg_controllen = sizeof(buf)
	};

#ifdef MSG_CMSG_CLOEXEC
	ssize_t nr = recvmsg(fd, &msg, MSG_NOSIGNAL | MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
#else
	ssize_t nr = recvmsg(fd, &msg, MSG_NOSIGNAL | MSG_DONTWAIT);
#endif
	switch (nr) {
	case -1:
		neb_syslogl(LOG_ERR, "recvmsg: %m"); /* fall through */
	case 0:
		return nr;
	default:
		break;
	}

	if (msg.msg_flags & MSG_CTRUNC) {
		neb_syslog(LOG_CRIT, "cmsg has ctrunc flag set");
		return -1;
	}

	*fd_num = 0;
	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
	if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
		neb_syslog(LOG_CRIT, "unexpected cmsg: level %d type %d", cmsg->cmsg_level, cmsg->cmsg_type);
		return -1;
	}
	payload_len = cmsg->cmsg_len - CMSG_LEN(0);
	*fd_num = payload_len / sizeof(int);
	if (*fd_num)
		memcpy(fds, CMSG_DATA(cmsg), payload_len);

#ifndef MSG_CMSG_CLOEXEC
	int err = 0;
	for (int i = 0; i < *fd_num; i++) {
		int set = 1;
		if (fcntl(fds[i], F_SETFD, FD_CLOEXEC, &set) == -1) {
			neb_syslogl(LOG_ERR, "fcntl(FD_CLOEXEC): %m");
			err = 1;
			break;
		}
	}
	if (err) {
		for (int i = 0; i < *fd_num; i++)
			close(fds[i]);
		*fd_num = 0;
		return -1;
	}
#endif
	return nr;
}
