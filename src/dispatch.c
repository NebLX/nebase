
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/dispatch.h>
#include <nebase/time.h>
#include <nebase/events.h>

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#if defined(OS_LINUX)
# include <sys/epoll.h>
# include <sys/timerfd.h>
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
# include <sys/types.h>
# include <sys/event.h>
# include <sys/time.h>
#elif defined(OS_SOLARIS)
# include <port.h>
# include <time.h>
# include <poll.h>
#endif

enum {
	DISPATCH_SOURCE_NONE,
	DISPATCH_SOURCE_FD,
	DISPATCH_SOURCE_ITIMER,
	DISPATCH_SOURCE_ABSTIMER,
};

struct dispatch_queue {
	int fd;
	batch_handler_t batch_call;
	void *udata;
};

struct dispatch_source_fd {
	int fd;
	io_handler_t read_call;
	io_handler_t write_call;
	io_handler_t hup_call;
};

struct dispatch_source_itimer {
	unsigned int ident;
	timer_handler_t timer_call;
	int64_t sec;
	int64_t msec;
#if defined(OS_LINUX)
	int fd;
#elif defined(OS_SOLARIS)
	timer_t timerid;
#endif
};

struct dispatch_source_abstimer {
	unsigned int ident;
	int sec_of_day;
	int interval_hour;
	timer_handler_t timer_call;
#if defined(OS_LINUX)
	int fd;
#elif defined(OS_SOLARIS)
	timer_t timerid;
#endif
};

struct dispatch_source {
	int type;
	int in_use;
	int re_add;
	void *udata;
	union {
		struct dispatch_source_fd s_fd;
		struct dispatch_source_itimer s_itimer;
		struct dispatch_source_abstimer s_abstimer;
	};
};

dispatch_queue_t neb_dispatch_queue_create(batch_handler_t bf, void* udata)
{
	dispatch_queue_t q = malloc(sizeof(struct dispatch_queue));
	if (!q) {
		neb_syslog(LOG_ERR, "malloc: %m");
		return NULL;
	}
	q->batch_call = bf;
	q->udata = udata;

#if defined(OS_LINUX)
	q->fd = epoll_create1(EPOLL_CLOEXEC);
	if (q->fd == -1) {
		neb_syslog(LOG_ERR, "epoll_create1: %m");
		free(q);
		return NULL;
	}
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	q->fd = kqueue();
	if (q->fd == -1) {
		neb_syslog(LOG_ERR, "kqueue: %m");
		free(q);
		return NULL;
	}
#elif defined(OS_SOLARIS)
	q->fd = port_create();
	if (q->fd == -1) {
		neb_syslog(LOG_ERR, "port_create: %m");
		free(q);
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
	// allow re-add/update
#if defined(OS_LINUX)
	struct epoll_event ee;
	ee.data.ptr = s;
	ee.events = EPOLLHUP;
	if (s->s_fd.read_call)
		ee.events |= EPOLLIN;
	if (s->s_fd.write_call)
		ee.events |= EPOLLOUT;
	if (epoll_ctl(q->fd, EPOLL_CTL_ADD, s->s_fd.fd, &ee) == -1) {
		if (errno == EEXIST) {
			if (epoll_ctl(q->fd, EPOLL_CTL_MOD, s->s_fd.fd, &ee) == -1) {
				neb_syslog(LOG_ERR, "epoll_ctl(MOD): %m");
				return -1;
			}
		} else {
			neb_syslog(LOG_ERR, "epoll_ctl(ADD): %m");
			return -1;
		}
	}
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	struct kevent ke;
	short filter = 0;
	if (s->s_fd.read_call)
		filter = EVFILT_READ;
	if (s->s_fd.write_call)
		filter = EVFILT_WRITE;     /* EV_EOF always set */
	EV_SET(&ke, s->s_fd.fd, filter, EV_ADD | EV_ENABLE, 0, 0, s);
	if (kevent(q->fd, &ke, 1, NULL, 0, NULL) == -1) { // updated if dup
		neb_syslog(LOG_ERR, "kevent: %m");
		return -1;
	}
#elif defined(OS_SOLARIS)
	int events = POLLHUP;
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

static int add_source_itimer(dispatch_queue_t q, dispatch_source_t s)
{
	// allow update
#if defined(OS_LINUX)
	if (s->s_itimer.fd == -1) {
		// create the timer
		s->s_itimer.fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
		if (s->s_itimer.fd == -1) {
			neb_syslog(LOG_ERR, "timerfd_create: %m");
			return -1;
		}
		// associate to epoll
		struct epoll_event ee;
		ee.data.ptr = s;
		ee.events = EPOLLIN;
		if (epoll_ctl(q->fd, EPOLL_CTL_ADD, s->s_itimer.fd, &ee) == -1) {
			neb_syslog(LOG_ERR, "epoll_ctl: %m");
			close(s->s_itimer.fd);
			return -1;
		}
	}
	// arm the timer
	int64_t sec = s->s_itimer.sec;
	int64_t nsec = (sec) ? 0 : (int64_t)s->s_itimer.msec * 1000000;
	struct itimerspec it;
	it.it_value.tv_sec = sec;
	it.it_value.tv_nsec = nsec;
	it.it_interval.tv_sec = sec;
	it.it_interval.tv_nsec = nsec;
	if (timerfd_settime(s->s_itimer.fd, 0, &it, NULL) == -1) {
		neb_syslog(LOG_ERR, "timerfd_settime: %m");
		close(s->s_itimer.fd);
		return -1;
	}
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	unsigned int fflags = 0;
	int64_t data = s->s_itimer.msec;
	if (!data) {
# ifdef NOTE_SECONDS
		fflags |= NOTE_SECONDS;
		data = s->s_itimer.sec;
# else
		data = (int64_t)s->s_itimer.sec * 1000;
# endif
	}
	struct kevent ke;
	EV_SET(&ke, s->s_itimer.ident, EVFILT_TIMER, EV_ADD | EV_ENABLE, fflags, data, s);
	if (kevent(q->fd, &ke, 1, NULL, 0, NULL) == -1) {
		neb_syslog(LOG_ERR, "kevent: %m");
		return -1;
	}
#elif defined(OS_SOLARIS)
	if (s->s_itimer.timerid == -1) {
		// create the timer and associate with the port
		port_notify_t pn = {
			.portnfy_port = q->fd,
			.portnfy_user = s
		};
		struct sigevent envp;
		envp.sigev_notify = SIGEV_PORT;
		envp.sigev_value.sival_ptr = &pn;
		if (timer_create(CLOCK_MONOTONIC, &envp, &s->s_itimer.timerid) == -1) {
			neb_syslog(LOG_ERR, "timer_create: %m");
			return -1;
		}
	}
	// arm the timer
	int64_t sec = s->s_itimer.sec;
	int64_t nsec = (sec) ? 0 : (int64_t)s->s_itimer.msec * 1000000;
	struct itimerspec it;
	it.it_value.tv_sec = sec;
	it.it_value.tv_nsec = nsec;
	it.it_interval.tv_sec = sec;
	it.it_interval.tv_nsec = nsec;
	if (timer_settime(s->s_itimer.timerid, 0, &it, NULL) == -1) {
		neb_syslog(LOG_ERR, "timer_settime: %m");
		timer_delete(s->s_itimer.timerid);
		return -1;
	}
#else
# error "fix me"
#endif
	return 0;
}

static int add_source_abstimer(dispatch_queue_t q, dispatch_source_t s)
{
	// allow re-add/update
	time_t abs_ts;
	int delta_sec;
	if (neb_daytime_abs_nearest(s->s_abstimer.sec_of_day, &abs_ts, &delta_sec) != 0) {
		neb_syslog(LOG_ERR, "Failed to get next abs time for sec_of_day %d", s->s_abstimer.sec_of_day);
		return -1;
	}
#if defined(OS_LINUX)
	time_t interval_sec = (time_t)s->s_abstimer.interval_hour * 3600;
	if (s->s_abstimer.fd == -1) {
		// create the timer
		s->s_abstimer.fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
		if (s->s_abstimer.fd == -1) {
			neb_syslog(LOG_ERR, "timerfd_create: %m");
			return -1;
		}
		// associate to epoll
		struct epoll_event ee;
		ee.data.ptr = s;
		ee.events = EPOLLIN;
		if (epoll_ctl(q->fd, EPOLL_CTL_ADD, s->s_abstimer.fd, &ee) == -1) {
			neb_syslog(LOG_ERR, "epoll_ctl: %m");
			close(s->s_abstimer.fd);
			return -1;
		}
	}
	// arm the timer
	struct itimerspec it;
	it.it_value.tv_sec = abs_ts;
	it.it_value.tv_nsec = 0;
	it.it_interval.tv_sec = interval_sec;
	it.it_interval.tv_nsec = 0;
	if (timerfd_settime(s->s_abstimer.fd, TFD_TIMER_ABSTIME, &it, NULL) == -1) {
		neb_syslog(LOG_ERR, "timerfd_settime: %m");
		close(s->s_abstimer.fd);
		return -1;
	}
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	unsigned int fflags = 0;
	int64_t data = 0;
# if defined(NOTE_ABSTIME)
	fflags = NOTE_ABSTIME | NOTE_SECONDS;
	data = abs_ts;
# elif defined(NOTE_SECONDS)
	fflags = NOTE_SECONDS;
	data = delta_sec;
# else
	data = (int64_t)delta_sec * 1000;
# endif
	struct kevent ke; // NOTE we only set the first run time here
	EV_SET(&ke, s->s_abstimer.ident, EVFILT_TIMER, EV_ADD | EV_ENABLE | EV_ONESHOT, fflags, data, s);
	if (kevent(q->fd, &ke, 1, NULL, 0, NULL) == -1) {
		neb_syslog(LOG_ERR, "kevent: %m");
		return -1;
	}
#elif defined(OS_SOLARIS)
	time_t interval_sec = (time_t)s->s_abstimer.interval_hour * 3600;
	if (s->s_abstimer.timerid == -1) {
		// create the timer and associate with the port
		port_notify_t pn = {
			.portnfy_port = q->fd,
			.portnfy_user = s
		};
		struct sigevent envp;
		envp.sigev_notify = SIGEV_PORT;
		envp.sigev_value.sival_ptr = &pn;
		if (timer_create(CLOCK_REALTIME, &envp, &s->s_abstimer.timerid) == -1) {
			neb_syslog(LOG_ERR, "timer_create: %m");
			return -1;
		}
	}
	// arm the timer
	struct itimerspec it;
	it.it_value.tv_sec = abs_ts;
	it.it_value.tv_nsec = 0;
	it.it_interval.tv_sec = interval_sec;
	it.it_interval.tv_nsec = 0;
	if (timer_settime(s->s_abstimer.timerid, TIMER_ABSTIME, &it, NULL) == -1) {
		neb_syslog(LOG_ERR, "timer_settime: %m");
		timer_delete(s->s_abstimer.timerid);
		return -1;
	}
#else
# error "fix me"
#endif
	return 0;
}

int neb_dispatch_queue_add(dispatch_queue_t q, dispatch_source_t s)
{
	int ret = 0;
	// allow re-add/update
	if (!s->re_add && s->in_use)
		return ret;
	s->re_add = 0;
	switch (s->type) {
	case DISPATCH_SOURCE_FD:
		ret = add_source_fd(q, s);
		break;
	case DISPATCH_SOURCE_ITIMER:
		ret = add_source_itimer(q, s);
		break;
	case DISPATCH_SOURCE_ABSTIMER:
		ret = add_source_abstimer(q, s);
		break;
	case DISPATCH_SOURCE_NONE: /* fall through */
	default:
		neb_syslog(LOG_ERR, "Invalid source type");
		ret = -1;
		break;
	}
	if (ret == 0)
		s->in_use = 1;
	return ret;
}

static int rm_source_fd(dispatch_queue_t q, dispatch_source_t s)
{
	// no resource validation as we use in_use as guard
#if defined(OS_LINUX)
	if (epoll_ctl(q->fd, EPOLL_CTL_DEL, s->s_fd.fd, NULL) == -1) {
		neb_syslog(LOG_ERR, "epoll_ctl: %m");
		return -1;
	}
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	struct kevent ke;
	short filter = 0;
	if (s->s_fd.read_call)
		filter = EVFILT_READ;
	if (s->s_fd.write_call)
		filter = EVFILT_WRITE;
	EV_SET(&ke, s->s_fd.fd, filter, EV_DISABLE | EV_DELETE, 0, 0, s);
	if (kevent(q->fd, &ke, 1, NULL, 0, NULL) == -1) {
		neb_syslog(LOG_ERR, "kevent: %m");
		return -1;
	}
#elif defined(OS_SOLARIS)
	if (port_dissociate(q->fd, PORT_SOURCE_FD, s->s_fd.fd) == -1) {
		neb_syslog(LOG_ERR, "port_dissociate: %m");
		return -1;
	}
#else
# error "fix me"
#endif
	return 0;
}

static int rm_source_itimer(dispatch_queue_t q, dispatch_source_t s)
{
	int ret = 0;
	// no resource validation as we use in_use as guard
#if defined(OS_LINUX)
	if (epoll_ctl(q->fd, EPOLL_CTL_DEL, s->s_itimer.fd, NULL) == -1) {
		neb_syslog(LOG_ERR, "epoll_ctl: %m");
		ret = -1;
	} else {
		close(s->s_itimer.fd);
		s->s_itimer.fd = -1;
	}
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	struct kevent ke;
	EV_SET(&ke, s->s_itimer.ident, EVFILT_TIMER, EV_DISABLE | EV_DELETE, 0, 0, NULL);
	if (kevent(q->fd, &ke, 1, NULL, 0, NULL) == -1) {
		neb_syslog(LOG_ERR, "kevent: %m");
		ret = -1;
	}
#elif defined(OS_SOLARIS)
	if (timer_delete(s->s_itimer.timerid) == -1) {
		neb_syslog(LOG_ERR, "timer_delete: %m");
		ret = -1;
	} else {
		s->s_itimer.timerid = -1;
	}
#else
# error "fix me"
#endif
	return ret;
}

static int rm_source_abstimer(dispatch_queue_t q, dispatch_source_t s)
{
	int ret = 0;
	// no resource validation as we use in_use as guard
#if defined(OS_LINUX)
	if (epoll_ctl(q->fd, EPOLL_CTL_DEL, s->s_abstimer.fd, NULL) == -1) {
		neb_syslog(LOG_ERR, "epoll_ctl: %m");
		ret = -1;
	} else {
		close(s->s_abstimer.fd);
		s->s_abstimer.fd = -1;
	}
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	struct kevent ke;
	EV_SET(&ke, s->s_abstimer.ident, EVFILT_TIMER, EV_DISABLE | EV_DELETE, 0, 0, NULL);
	if (kevent(q->fd, &ke, 1, NULL, 0, NULL) == -1 && errno != ENOENT) {
		neb_syslog(LOG_ERR, "kevent: %m");
		ret = -1;
	}
#elif defined(OS_SOLARIS)
	if (timer_delete(s->s_abstimer.timerid) == -1) {
		neb_syslog(LOG_ERR, "timer_delete: %m");
		ret = -1;
	} else {
		s->s_abstimer.timerid = -1;
	}
#else
# error "fix me"
#endif
	return ret;
}

int neb_dispatch_queue_rm(dispatch_queue_t q, dispatch_source_t s)
{
	int ret = 0;
	if (!s->in_use)
		return ret;
	switch (s->type) {
	case DISPATCH_SOURCE_FD:
		ret = rm_source_fd(q, s);
		break;
	case DISPATCH_SOURCE_ITIMER:
		ret = rm_source_itimer(q, s);
		break;
	case DISPATCH_SOURCE_ABSTIMER:
		ret = rm_source_abstimer(q, s);
		break;
	case DISPATCH_SOURCE_NONE: /* fall through */
	default:
		neb_syslog(LOG_ERR, "Invalid source type");
		ret = -1;
		break;
	}
	s->in_use = 0;
	return ret;
}

int neb_dispatch_source_del(dispatch_source_t s)
{
	if (s->in_use) {
		neb_syslog(LOG_ERR, "source is currently in use");
		return -1;
	}

	free(s);
	return 0;
}

dispatch_source_t neb_dispatch_source_new_fd_read(int fd, io_handler_t rf, io_handler_t hf, void *udata)
{
	struct dispatch_source *s = calloc(1, sizeof(struct dispatch_source));
	if (!s) {
		neb_syslog(LOG_ERR, "calloc: %m");
		return NULL;
	}
	s->type = DISPATCH_SOURCE_FD;
	s->s_fd.fd = fd;
	s->s_fd.read_call = rf;
	s->s_fd.hup_call = hf;
	s->udata = udata;
	return s;
}

dispatch_source_t neb_dispatch_source_new_fd_write(int fd, io_handler_t wf, io_handler_t hf, void *udata)
{
	struct dispatch_source *s = calloc(1, sizeof(struct dispatch_source));
	if (!s) {
		neb_syslog(LOG_ERR, "calloc: %m");
		return NULL;
	}
	s->type = DISPATCH_SOURCE_FD;
	s->s_fd.fd = fd;
	s->s_fd.write_call = wf;
	s->s_fd.hup_call = hf;
	s->udata = udata;
	return s;
}

dispatch_source_t neb_dispatch_source_new_itimer_sec(unsigned int ident, int64_t sec, timer_handler_t tf, void *udata)
{
	struct dispatch_source *s = calloc(1, sizeof(struct dispatch_source));
	if (!s) {
		neb_syslog(LOG_ERR, "calloc: %m");
		return NULL;
	}
	s->type = DISPATCH_SOURCE_ITIMER;
	s->s_itimer.ident = ident;
	s->s_itimer.sec = sec;
	s->s_itimer.timer_call = tf;
	s->udata = udata;
	return s;
}

dispatch_source_t neb_dispatch_source_new_itimer_msec(unsigned int ident, int64_t msec, timer_handler_t tf, void *udata)
{
	struct dispatch_source *s = calloc(1, sizeof(struct dispatch_source));
	if (!s) {
		neb_syslog(LOG_ERR, "calloc: %m");
		return NULL;
	}
	s->type = DISPATCH_SOURCE_ITIMER;
	s->s_itimer.ident = ident;
	s->s_itimer.msec = msec;
	s->s_itimer.timer_call = tf;
#if defined(OS_LINUX)
	s->s_itimer.fd = -1;
#elif defined(OS_SOLARIS)
	s->s_itimer.timerid = -1;
#endif
	s->udata = udata;
	return s;
}

dispatch_source_t neb_dispatch_source_new_abstimer(unsigned int ident, int sec_of_day, int interval_hour, timer_handler_t tf, void *udata)
{
	struct dispatch_source *s = calloc(1, sizeof(struct dispatch_source));
	if (!s) {
		neb_syslog(LOG_ERR, "calloc: %m");
		return NULL;
	}
	s->type = DISPATCH_SOURCE_ABSTIMER;
	s->s_abstimer.ident = ident;
	s->s_abstimer.sec_of_day = sec_of_day;
	s->s_abstimer.interval_hour = interval_hour;
	s->s_abstimer.timer_call = tf;
#if defined(OS_LINUX)
	s->s_abstimer.fd = -1;
#elif defined(OS_SOLARIS)
	s->s_abstimer.timerid = -1;
#endif
	s->udata = udata;
	return s;
}

static dispatch_cb_ret_t handle_source_fd(dispatch_source_t s, void *event)
{
	dispatch_cb_ret_t ret = DISPATCH_CB_CONTINUE;
	int eread = 0, ewrite = 0, ehup = 0;
#if defined(OS_LINUX)
	struct epoll_event *e = event;
	ehup = e->events & EPOLLHUP;
	eread = e->events & EPOLLIN;
	ewrite = e->events & EPOLLOUT;
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	struct kevent *e = event;
	ehup = e->flags & EV_EOF;
	eread = (e->filter == EVFILT_READ);
	ewrite = (e->filter == EVFILT_WRITE);
#elif defined(OS_SOLARIS)
	port_event_t *e = event;
	ehup = e->portev_events & POLLHUP;
	eread = e->portev_events & POLLIN;
	ewrite = e->portev_events & POLLOUT;
#else
# error "fix me"
#endif
	if (eread) {
		if (!s->s_fd.read_call)
			ret = DISPATCH_CB_REMOVE;
		else
			ret = s->s_fd.read_call(s->s_fd.fd, s->udata);
		if (ret != DISPATCH_CB_CONTINUE)
			goto exit_return;
	}
	if (ehup) {
		if (s->s_fd.hup_call)
			ret = s->s_fd.hup_call(s->s_fd.fd, s->udata);
		if (ret != DISPATCH_CB_BREAK)
			ret = DISPATCH_CB_REMOVE;
		goto exit_return;
	}
	if (ewrite) {
		if (!s->s_fd.write_call)
			ret = DISPATCH_CB_REMOVE;
		else
			ret = s->s_fd.write_call(s->s_fd.fd, s->udata);
	}
exit_return:
#if defined(OS_SOLARIS)
	if (ret == DISPATCH_CB_CONTINUE)
		ret = DISPATCH_CB_READD;
#endif
	return ret;
}

static dispatch_cb_ret_t handle_source_itimer(dispatch_source_t s)
{
	int ret = DISPATCH_CB_CONTINUE;
	if (s->s_abstimer.timer_call)
		ret = s->s_abstimer.timer_call(s->s_abstimer.ident, s->udata);
	else
		ret = DISPATCH_CB_REMOVE;
	return ret;
}

static dispatch_cb_ret_t handle_source_abstimer(dispatch_source_t s)
{
	int ret = DISPATCH_CB_CONTINUE;
	if (s->s_abstimer.timer_call)
		ret = s->s_abstimer.timer_call(s->s_abstimer.ident, s->udata);
	else
		ret = DISPATCH_CB_REMOVE;

	if (ret != DISPATCH_CB_CONTINUE)
		return ret;
	else
		return DISPATCH_CB_READD;
}

static dispatch_cb_ret_t handle_event(dispatch_queue_t q, void *event)
{
	dispatch_cb_ret_t ret = DISPATCH_CB_CONTINUE;
	dispatch_source_t s = NULL;
#if defined(OS_LINUX)
	struct epoll_event *e = event;
	s = e->data.ptr;
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	struct kevent *e = event;
	s = e->udata;
#elif defined(OS_SOLARIS)
	port_event_t *e = event;
	s = e->portev_user;
#else
# error "fix me"
#endif

	switch (s->type) {
	case DISPATCH_SOURCE_FD:
		ret = handle_source_fd(s, event);
		break;
	case DISPATCH_SOURCE_ITIMER:
		ret = handle_source_itimer(s);
		break;
	case DISPATCH_SOURCE_ABSTIMER:
		ret = handle_source_abstimer(s);
		break;
	default:
		neb_syslog(LOG_ERR, "Unsupport dispatch source type %d", s->type);
		ret = DISPATCH_CB_BREAK;
		break;
	}

	switch (ret) {
	case DISPATCH_CB_REMOVE:
		if (neb_dispatch_queue_rm(q, s) != 0) {
			neb_syslog(LOG_ERR, "Failed to remove source"); // TODO add a source descriptor
			ret = DISPATCH_CB_BREAK;
		} else {
			ret = DISPATCH_CB_CONTINUE;
		}
		break;
	case DISPATCH_CB_READD:
		s->re_add = 0;
		if (neb_dispatch_queue_add(q, s) != 0) {
			neb_syslog(LOG_ERR, "Failed to readd source"); // TODO
			ret = DISPATCH_CB_BREAK;
		} else {
			ret = DISPATCH_CB_CONTINUE;
		}
		break;
	default:
		break;
	}

	return ret;
}

int neb_dispatch_queue_run(dispatch_queue_t q, tevent_handler_t tef, void *udata)
{
#define BATCH_EVENTS 20
	int ret = 0;
	for (;;) {
		if (thread_events) {
			if (tef) {
				if (tef(udata) == DISPATCH_CB_BREAK)
					break;
			} else {
				if (thread_events & T_E_QUIT)
					break;

				thread_events = 0;
			}
		}
		int events;
#if defined(OS_LINUX)
		struct epoll_event ee[BATCH_EVENTS];
		events = epoll_wait(q->fd, ee, BATCH_EVENTS, -1);
		if (events == -1) {
			switch (errno) {
			case EINTR:
				continue;
				break;
			default:
				neb_syslog(LOG_ERR, "epoll_wait: %m");
				ret = -1;
				goto exit_return;
			}
		}
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
		struct kevent ee[BATCH_EVENTS];
		events = kevent(q->fd, NULL, 0, ee, BATCH_EVENTS, NULL);
		if (events == -1) {
			switch (errno) {
			case EINTR:
				continue;
				break;
			default:
				neb_syslog(LOG_ERR, "kevent: %m");
				ret = -1;
				goto exit_return;
			}
		}
#elif defined(OS_SOLARIS)
		port_event_t ee[BATCH_EVENTS];
		uint_t nget = 1;
		if (port_getn(q->fd, ee, BATCH_EVENTS, &nget, NULL) == -1) {
			switch(errno) {
			case EINTR:
				continue;
				break;
			default:
				neb_syslog(LOG_ERR, "port_getn: %m");
				ret = -1;
				goto exit_return;
			}
		}
		events = nget;
#else
# error "fix me"
#endif
		for (int i = 0; i < events; i++) {
			if (handle_event(q, ee + i) == DISPATCH_CB_BREAK)
				goto exit_return;
		}
		if (q->batch_call) {
			if (q->batch_call(q->udata) == DISPATCH_CB_BREAK)
				goto exit_return;
		}
	}
exit_return:
	return ret;
}
