
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/sock.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <errno.h>

#if defined(OS_LINUX)
# define NEB_SIZE_UCRED sizeof(struct ucred)
# define NEB_SCM_CREDS SCM_CREDENTIALS
#elif defined(OS_FREEBSD) || defined(OS_DRAGONFLY)
# define NEB_SIZE_UCRED sizeof(struct cmsgcred)
# define NEB_SCM_CREDS SCM_CREDS
#elif defined(OS_NETBSD)
# define NEB_SIZE_UCRED SOCKCREDSIZE(0)
# define NEB_SCM_CREDS SCM_CREDS
#elif defined(OS_SOLARIS)
//FIXME only dgram works for socketpair
//  currently we have not test listen/connect
# include <ucred.h>
# define NEB_SIZE_UCRED ucred_size()
# define NEB_SCM_CREDS SCM_UCRED
#elif defined(OS_OPENBSD)
//FIXME work with listen/connect sockets only, no socketpair support
//  we need to wait for upstream support
#elif defined(OS_DARWIN)
# include <sys/ucred.h>
# ifndef MSG_NOSIGNAL
#  define MSG_NOSIGNAL 0
# endif
#else
# error "fix me"
#endif

#if defined(OS_LINUX) || defined(OS_SOLARIS)
int neb_sock_unix_enable_recv_cred(int fd)
{
# if defined(OS_LINUX)
	int passcred = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &passcred, sizeof(passcred)) == -1) {
		neb_syslog(LOG_ERR, "setsockopt(SO_PASSCRED): %m");
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

#if defined(OS_FREEBSD) || defined(OS_DRAGONFLY) || defined(OS_NETBSD)
int neb_sock_unix_send_with_cred(int fd, const char *data, int len)
{
	struct iovec iov = {
		.iov_base = (void *)data,
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

	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = NEB_SCM_CREDS;
	cmsg->cmsg_len = CMSG_LEN(NEB_SIZE_UCRED);

# if defined(OS_FREEBSD) || defined(OS_DRAGONFLY)
	struct cmsgcred *u = (struct cmsgcred *)CMSG_DATA(cmsg);
	memset(u, 0, NEB_SIZE_UCRED);
# elif defined(OS_NETBSD)
	struct sockcred *u = (struct sockcred *)CMSG_DATA(cmsg);
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
int neb_sock_unix_send_with_cred(int fd, const char *data, int len)
{
	struct iovec iov = {
		.iov_base = (void *)data,
		.iov_len = len
	};
	struct msghdr msg = {
		.msg_name = NULL,
		.msg_namelen = 0,
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

#if defined(OS_OPENBSD) || defined(OS_DARWIN)
int neb_sock_unix_recv_with_cred(int fd, char *data, int len, struct neb_ucred *pu)
{
# if defined(OS_OPENBSD)
	struct sockpeercred scred;
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
# elif defined(OS_FREEBSD) || defined(OS_DRAGONFLY)
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
