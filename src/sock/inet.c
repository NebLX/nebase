
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/sock/inet.h>
#include <nebase/time.h>

#include <netinet/in.h>

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

int neb_sock_inet_recv_with_time(int fd, struct sockaddr *addr, char *data, int len, struct timespec *ts)
{
	void *name = addr;
	socklen_t namelen = 0;
	if (name) {
		switch (addr->sa_family) {
		case AF_INET:
			namelen = sizeof(struct sockaddr_in);
			break;
		case AF_INET6:
			namelen = sizeof(struct sockaddr_in6);
			break;
		default:
			neb_syslog(LOG_ERR, "Unsupported address family %d", addr->sa_family);
			return -1;
			break;
		}
	}

	struct iovec iov = {
		.iov_base = data,
		.iov_len = len
	};

	// NOTE order should be the same as SO_* in neb_sock_net_enable_recv_time
#if defined(SCM_TIMESTAMPNS)
	char buf[CMSG_SPACE(sizeof(struct timespec))];
#elif defined(SCM_BINTIME)
	char buf[CMSG_SPACE(sizeof(struct bintime))];
#elif defined(SCM_TIMESTAMP)
	char buf[CMSG_SPACE(sizeof(struct timeval))];
#else
# error "fix me"
#endif
	struct msghdr msg = {
		.msg_name = name,
		.msg_namelen = namelen,
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = buf,
		.msg_controllen = sizeof(buf)
	};

	ssize_t nr = recvmsg(fd, &msg, MSG_NOSIGNAL | MSG_DONTWAIT);
	switch (nr) {
	case -1:
		neb_syslog(LOG_ERR, "recvmsg: %m"); /* fall through */
	case 0:
		return nr;
	default:
		break;
	}

	if (msg.msg_flags & MSG_CTRUNC) {
		neb_syslog(LOG_CRIT, "cmsg has ctrunc flag set");
		return -1;
	}

	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
	if (!cmsg || cmsg->cmsg_level != SOL_SOCKET) {
		// no cmsg available, just get the real time
		if (neb_time_gettimeofday(ts) != 0)
			return -1;
	} else {
		switch (cmsg->cmsg_type) {
#ifdef SCM_TIMESTAMPNS
		case SCM_TIMESTAMPNS:
		{
			const struct timespec *t = (const struct timespec *)CMSG_DATA(cmsg);
			ts->tv_sec = t->tv_sec;
			ts->tv_nsec = t->tv_nsec;
		}
			break;
#endif
#ifdef SCM_BINTIME
		case SCM_BINTIME:
		{
			const struct bintime *bt = (const struct bintime *)CMSG_DATA(cmsg);
			BINTIME_TO_TIMESPEC(bt, ts);
		}
			break;
#endif
#ifdef SCM_TIMESTAMP
		case SCM_TIMESTAMP:
		{
			const struct timeval *tv = (const struct timeval *)CMSG_DATA(cmsg);
			TIMEVAL_TO_TIMESPEC(tv, ts);
		}
			break;
#endif
		default:
			neb_syslog(LOG_CRIT, "unexpected cmsg_type %d", cmsg->cmsg_type);
			return -1;
			break;
		}
	}

	return nr;
}
