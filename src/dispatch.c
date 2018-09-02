
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/dispatch.h>

#include <stdlib.h>
#include <unistd.h>

#if defined(OS_LINUX)
# include <sys/epoll.h>
# include <sys/timerfd.h>
#elif defined(OSTYPE_BSD)
# include <sys/types.h>
# include <sys/event.h>
# include <sys/time.h>
#elif defined(OS_SOLARIS)
# include <port.h>
#endif

enum {
	DISPATCH_SOURCE_NONE,
	DISPATCH_SOURCE_FD,
	DISPATCH_SOURCE_TIMER
};

struct dispatch_queue {
	int fd;
};

struct dispatch_source_fd {
	int fd;
	io_handler_t read_call;
	io_handler_t write_call;
	io_handler_t hup_call;
};

struct dispatch_source_timer {
	int ident;
#if defined(OS_LINUX)
	int fd;
#elif defined(OS_SOLARIS)
	timer_t timerid;
#else
# error "fix me"
#endif
};

struct dispatch_source {
	int type;
	union {
		struct dispatch_source_fd s_fd;
		struct dispatch_source_timer s_timer;
	};
};

dispatch_queue_t neb_dispatch_queue_create(void)
{
	dispatch_queue_t q = malloc(sizeof(struct dispatch_queue));
	if (!q) {
		neb_syslog(LOG_ERR, "malloc: %m");
		return NULL;
	}

#if defined(OS_LINUX)
	q->fd = epoll_create1(EPOLL_CLOEXEC);
	if (q->fd == -1) {
		neb_syslog(LOG_ERR, "epoll_create1: %m");
		return NULL;
	}
#elif defined(OSTYPE_BSD)
	q->fd = kqueue();
	if (q->fd == -1) {
		neb_syslog(LOG_ERR, "kqueue: %m");
		return NULL;
	}
#elif defined(OS_SOLARIS)
	q->fd = port_create();
	if (q->fd == -1) {
		neb_syslog(LOG_ERR, "port_create: %m");
		return NULL;
	}
#else
# error "fix me"
#endif

	return q;
}

void neb_dispatch_queue_destroy(dispatch_queue_t q)
{
	close(q->fd);
	free(q);
}

static int add_source_fd(dispatch_queue_t q, dispatch_source_t s)
{
#if defined(OS_LINUX)
	struct epoll_event ee;
	ee.data.ptr = s;
	ee.events = EPOLLHUP;
	if (s->s_fd.read_call)
		ee.events |= EPOLLIN;
	if (s->s_fd.write_call)
		ee.events |= EPOLLOUT;
	if (epoll_ctl(q->fd, EPOLL_CTL_ADD, s->s_fd.fd, &ee) == -1) {
		neb_syslog(LOG_ERR, "epoll_ctl: %m");
		return -1;
	}
#elif defined(OSTYPE_BSD)
	struct kevent ke;
	short filter = 0;
	if (s->s_fd.read_call)
		filter = EVFILT_READ;
	if (s->s_fd.write_call)
		filter = EVFILT_WRITE;     /* EV_EOF always set */
	EV_SET(&ke, s->s_fd.fd, filter, EV_ADD | EV_ENABLE, 0, 0, s);
	if (kevent(q->fd, &ke, 1, NULL, 0, NULL) == -1) {
		neb_syslog(LOG_ERR, "kevent: %m");
		return -1;
	}
#elif defined(OS_SOLARIS)
	int events = POLLHUP
	if (s->s_fd.read_call)
		events |= POLLIN;
	if (s->s_fd.write_call)
		events |= POLLOUT;
	if (port_associate(q->fd, PORT_SOURCE_FD, s->s_fd.fd, events, s) == -1) {
		neb_syslog(LOG_ERR, "port_associate: %m");
		return -1;
	}
#else
# error "fix me"
#endif
	return 0;
}

static int add_source_timer(dispatch_queue_t q, dispatch_source_t s)
{
#if defined(OS_LINUX)
	// create the timer
	s->s_timer.fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	if (s->s_timer.fd == -1) {
		neb_syslog(LOG_ERR, "timerfd_create: %m");
		return -1;
	}
	// associate to epoll
	struct epoll_event ee;
	ee.data.ptr = s;
	ee.events = EPOLLIN;
	if (epoll_ctl(q->fd, EPOLL_CTL_ADD, s->s_timer.fd, &ee) == -1) {
		neb_syslog(LOG_ERR, "epoll_ctl: %m");
		return -1;
	}
	// arm the timer
	// TODO
#elif defined(OSTYPE_BSD)
	int msec = 0; //TODO calc msec
	struct kevent ke;
	EV_SET(&ke, s->s_timer.ident, EVFILT_TIMER, EV_ADD | EV_ENABLE, 0, msec, 0);
	if (kevent(q->fd, &ke, 1, NULL, 0, NULL) == -1) {
		neb_syslog(LOG_ERR, "kevent: %m");
		return -1;
	}
#elif defined(OS_SOLARIS)
	// create the timer and associate with the port
	port_notify_t pn = {
		.portnfy_port = q->fd,
		.portnfy_user = s
	};
	struct sigevent envp;
	envp.sigev_notify = SIGEV_PORT;
	envp.sigev_value.sival_ptr = &pn;
	if (timer_create(CLOCK_MONOTONIC, &envp, &s->s_timer.timerid) == -1) {
		neb_syslog(LOG_ERR, "timer_create: %m");
		return -1;
	}
	// arm the timer
	// TODO
#else
# error "fix me"
#endif
	return 0;
}

int neb_dispatch_queue_add(dispatch_queue_t q, dispatch_source_t s)
{
	int ret = 0;
	switch (s->type) {
	case DISPATCH_SOURCE_FD:
		ret = add_source_fd(q, s);
		break;
	case DISPATCH_SOURCE_TIMER:
		ret = add_source_timer(q, s);
		break;
	case DISPATCH_SOURCE_NONE: /* fall through */
	default:
		neb_syslog(LOG_ERR, "Invalid source type");
		ret = -1;
		break;
	}
	return ret;
}
