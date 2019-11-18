
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/sock/inet.h>
#include <nebase/time.h>

#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>

#ifdef IP_RECVIF
# include <net/if_dl.h>
#endif

#define RECVMSG_CMSG_BUF_SIZE 10240 // see rfc3542 20.1

static int handle_cmsg(const struct cmsghdr *cmsg, neb_sock_cmsg_cb f, void *udata)
{
	switch (cmsg->cmsg_level) {
	case SOL_SOCKET:
		switch (cmsg->cmsg_type) {
#ifdef SCM_TIMESTAMPNS
		case SCM_TIMESTAMPNS:
		{
			const struct timespec *tp = (const struct timespec *)CMSG_DATA(cmsg);
			return f(NEB_CMSG_LEVEL_COMPAT, NEB_CMSG_TYPE_TIMESTAMP, (const u_char *)tp, sizeof(struct timespec), udata);
		}
			break;
#endif
#ifdef SCM_BINTIME
		case SCM_BINTIME:
		{
			struct timespec ts;
			const struct bintime *bt = (const struct bintime *)CMSG_DATA(cmsg);
			bintime2timespec(bt, &ts);
			return f(NEB_CMSG_LEVEL_COMPAT, NEB_CMSG_TYPE_TIMESTAMP, (const u_char *)&ts, sizeof(struct timespec), udata);
		}
			break;
#endif
#ifdef SCM_TIMESTAMP
		case SCM_TIMESTAMP:
		{
			struct timespec ts;
			const struct timeval *tv = (const struct timeval *)CMSG_DATA(cmsg);
			TIMEVAL_TO_TIMESPEC(tv, &ts);
			return f(NEB_CMSG_LEVEL_COMPAT, NEB_CMSG_TYPE_TIMESTAMP, (const u_char *)&ts, sizeof(struct timespec), udata);
		}
			break;
#endif
		default:
			break;
		}
		break;
	case IPPROTO_IP:
		switch (cmsg->cmsg_type) {
#ifdef IP_PKTINFO
		case IP_PKTINFO:
		{
			const struct in_pktinfo *info = (const struct in_pktinfo *)CMSG_DATA(cmsg);
			unsigned int ifindex = info->ipi_ifindex;
			return f(NEB_CMSG_LEVEL_COMPAT, NEB_CMSG_TYPE_IP4IFINDEX, (const u_char *)&ifindex, sizeof(ifindex), udata);
		}
			break;
#endif
#ifdef IP_RECVIF
		{
			const struct sockaddr_dl *addr = (const struct sockaddr_dl *)CMSG_DATA(cmsg);
			unsigned int ifindex = addr->sdl_index;
			return f(NEB_CMSG_LEVEL_COMPAT, NEB_CMSG_TYPE_IP4IFINDEX, (const u_char *)&ifindex, sizeof(ifindex), udata);
		}
#endif
		default:
			break;
		}
		break;
	default:
		break;
	}

	return f(cmsg->cmsg_level, cmsg->cmsg_type, CMSG_DATA(cmsg), cmsg->cmsg_len - CMSG_LEN(0), udata);
}

static ssize_t do_cmsg_recvmsg(int fd, struct msghdr *m, neb_sock_cmsg_cb f, void *udata)
{
	ssize_t nr = recvmsg(fd, m, MSG_NOSIGNAL | MSG_DONTWAIT);
	if (nr == -1) {
		neb_syslog(LOG_ERR, "recvmsg: %m");
		return -1;
	}

	if (m->msg_flags & MSG_CTRUNC) {
		neb_syslog(LOG_CRIT, "cmsg has ctrunc flag set");
		return -1;
	}

	for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(m); cmsg; cmsg = CMSG_NXTHDR(m, cmsg)) {
		int ret = handle_cmsg(cmsg, f, udata);
		if (ret != 0)
			return ret;
	}

	int ret = f(NEB_CMSG_LEVEL_COMPAT, NEB_CMSG_TYPE_LOOP_END, NULL, 0, udata);
	if (ret != 0)
		return ret;

	return nr;
}

static ssize_t do_plain_recvmsg(int fd, struct msghdr *m)
{
	ssize_t nr = recvmsg(fd, m, MSG_NOSIGNAL | MSG_DONTWAIT);
	if (nr == -1) {
		neb_syslog(LOG_ERR, "recvmsg: %m");
		return -1;
	}
	return nr;
}

ssize_t neb_sock_inet_recvmsg(int fd, struct neb_sock_msghdr *m)
{
	void *name = m->msg_peer;
	socklen_t namelen = 0;
	if (name) {
		switch (m->msg_peer->sa_family) {
		case AF_INET:
			namelen = sizeof(struct sockaddr_in);
			break;
		case AF_INET6:
			namelen = sizeof(struct sockaddr_in6);
			break;
		default:
			neb_syslog(LOG_ERR, "Unsupported address family %d", m->msg_peer->sa_family);
			return -1;
			break;
		}
	}

	if (m->msg_control_cb) {
		char buf[RECVMSG_CMSG_BUF_SIZE];
		struct msghdr msg = {
			.msg_name = name,
			.msg_namelen = namelen,
			.msg_iov = m->msg_iov,
			.msg_iovlen = m->msg_iovlen,
			.msg_control = buf,
			.msg_controllen = sizeof(buf),
		};
		return do_cmsg_recvmsg(fd, &msg, m->msg_control_cb, m->msg_udata);
	} else {
		struct msghdr msg = {
			.msg_name = name,
			.msg_namelen = namelen,
			.msg_iov = m->msg_iov,
			.msg_iovlen = m->msg_iovlen,
			.msg_control = NULL,
			.msg_controllen = 0,
		};
		return do_plain_recvmsg(fd, &msg);
	}
}

int neb_sock_inet_new(int domain, int type, int protocol)
{
#ifdef SOCK_NONBLOCK
	type |= SOCK_NONBLOCK;
#endif
#ifdef SOCK_CLOEXEC
	type |= SOCK_CLOEXEC;
#endif

	int fd = socket(domain, type, protocol);
	if (fd == -1) {
		neb_syslog(LOG_ERR, "socket(%d, %d, %d): %m", domain, type, protocol);
		return -1;
	}

#ifndef SOCK_NONBLOCK
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
		neb_syslog(LOG_ERR, "fcntl(F_SETFL, O_NONBLOCK): %m");
		close(fd);
		return -1;
	}
#endif
#ifndef SOCK_CLOEXEC
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
		neb_syslog(LOG_ERR, "fcntl(F_SETFD, FD_CLOEXEC): %m");
		close(fd);
		return -1;
	}
#endif

	return fd;
}

int neb_sock_inet_enable_recv_time(int fd)
{
	int enable = 1;
#if defined(SO_TIMESTAMPNS)
	if (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPNS, &enable, sizeof(enable)) == -1) {
		neb_syslog(LOG_ERR, "setsockopt(SCM_TIMESTAMPNS): %m");
		return -1;
	}
#elif defined(SO_BINTIME)
	if (setsockopt(fd, SOL_SOCKET, SO_BINTIME, &enable, sizeof(enable)) == -1) {
		neb_syslog(LOG_ERR, "setsockopt(SCM_TIMESTAMPNS): %m");
		return -1;
	}
#elif defined(SO_TIMESTAMP)
	if (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMP, &enable, sizeof(enable)) == -1) {
		neb_syslog(LOG_ERR, "setsockopt(SCM_TIMESTAMP): %m");
		return -1;
	}
#else
# error "fix me"
#endif
	return 0;
}
