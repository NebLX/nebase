
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

#include <glib.h>

enum {
	DISPATCH_SOURCE_NONE,
	DISPATCH_SOURCE_FD,
	DISPATCH_SOURCE_ITIMER,
	DISPATCH_SOURCE_ABSTIMER,
};

struct dispatch_queue {
	int fd;
	int batch_size;
	int total_events;
	int current_event; // start from 0, so should be < total_events
#if defined(OS_LINUX)
	struct epoll_event *ee;
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	struct kevent *ee;
#elif defined(OS_SOLARIS)
	port_event_t *ee;
#else
# error "fix me"
#endif
	batch_handler_t batch_call;
	GHashTable *sources;
	dispatch_source_t *re_add_sources;
	int re_add_num;
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
	int is_re_add;
	int re_add_immediatly;
	dispatch_queue_t q_in_use; // reference
	void *udata;
	source_cb_t on_remove;
	union {
		struct dispatch_source_fd s_fd;
		struct dispatch_source_itimer s_itimer;
		struct dispatch_source_abstimer s_abstimer;
	};
};

static int dispatch_queue_rm_internal(dispatch_queue_t q, dispatch_source_t s);

static void free_dispatch_source(void *p)
{
	dispatch_source_t s = p;
	if (s->on_remove)
		s->on_remove(s);
}

dispatch_queue_t neb_dispatch_queue_create(batch_handler_t bf, int batch_size, void* udata)
{
	if (batch_size <= 0)
		batch_size = NEB_DISPATCH_DEFAULT_BATCH_SIZE;
	dispatch_queue_t q = calloc(1, sizeof(struct dispatch_queue));
	if (!q) {
		neb_syslog(LOG_ERR, "malloc: %m");
		return NULL;
	}
	q->sources = g_hash_table_new_full(g_int64_hash, g_int64_equal, free, free_dispatch_source);
	if (!q->sources) {
		neb_syslog(LOG_ERR, "g_hash_table_new_full failed");
		free(q);
		return NULL;
	}
	q->batch_call = bf;
	q->udata = udata;
	q->batch_size = batch_size;

#if defined(OS_LINUX)
	q->ee = malloc(q->batch_size * sizeof(struct epoll_event));
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	q->ee = malloc(q->batch_size * sizeof(struct kevent));
#elif defined(OS_SOLARIS)
	q->ee = malloc(q->batch_size * sizeof(port_event_t));
#else
# error "fix me"
#endif
	if (!q->ee) {
		neb_syslog(LOG_ERR, "malloc: %m");
		g_hash_table_destroy(q->sources);
		free(q);
		return NULL;
	}

	q->re_add_sources = calloc(q->batch_size, sizeof(dispatch_source_t));
	if (!q->re_add_sources) {
		neb_syslog(LOG_ERR, "calloc: %m");
		free(q->ee);
		g_hash_table_destroy(q->sources);
		free(q);
		return NULL;
	}
	q->re_add_num = 0;

#if defined(OS_LINUX)
	q->fd = epoll_create1(EPOLL_CLOEXEC);
	if (q->fd == -1) {
		neb_syslog(LOG_ERR, "epoll_create1: %m");
		free(q->re_add_sources);
		free(q->ee);
		g_hash_table_destroy(q->sources);
		free(q);
		return NULL;
	}
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	q->fd = kqueue();
	if (q->fd == -1) {
		neb_syslog(LOG_ERR, "kqueue: %m");
		free(q->re_add_sources);
		free(q->ee);
		g_hash_table_destroy(q->sources);
		free(q);
		return NULL;
	}
#elif defined(OS_SOLARIS)
	q->fd = port_create();
	if (q->fd == -1) {
		neb_syslog(LOG_ERR, "port_create: %m");
		free(q->re_add_sources);
		free(q->ee);
		g_hash_table_destroy(q->sources);
		free(q);
		return NULL;
	}
#else
# error "fix me"
#endif

	return q;
}

static gboolean remove_source_from_q(gpointer k __attribute_unused__, gpointer v, gpointer u)
{
	dispatch_queue_t q = u;
	dispatch_source_t s = v;

	dispatch_queue_rm_internal(q, s);
	return TRUE;
}

void neb_dispatch_queue_destroy(dispatch_queue_t q)
{
	g_hash_table_foreach_remove(q->sources, remove_source_from_q, q);
	g_hash_table_destroy(q->sources);
	if (q->ee)
		free(q->ee);
	if (q->re_add_sources)
		free(q->re_add_sources);
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
	int re_add = s->is_re_add;
	// allow re-add/update
	if (!s->is_re_add && s->q_in_use)
		return ret;
	s->is_re_add = 0;
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
		neb_syslog(LOG_ERR, "Invalid source type %d", s->type);
		ret = -1;
		break;
	}
	if (ret == 0) {
		s->q_in_use = q;
		if (!re_add) {
			int64_t *k = malloc(sizeof(int64_t));
			if (!k) {
				neb_syslog(LOG_ERR, "malloc: %m");
				dispatch_queue_rm_internal(q, s);
				ret = -1;
			} else {
				*k = (int64_t)s;
				g_hash_table_replace(q->sources, k, s);
			}
		}
	}
	return ret;
}

static int rm_source_fd(dispatch_queue_t q, dispatch_source_t s)
{
	// no resource validation as we use in_use as guard
#if defined(OS_LINUX)
	if (epoll_ctl(q->fd, EPOLL_CTL_DEL, s->s_fd.fd, NULL) == -1) {
		neb_syslog(LOG_ERR, "(epoll %d)epoll_ctl: %m", q->fd);
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
		neb_syslog(LOG_ERR, "(kqueue %d)kevent: %m", q->fd);
		return -1;
	}
#elif defined(OS_SOLARIS)
	if (port_dissociate(q->fd, PORT_SOURCE_FD, s->s_fd.fd) == -1 && errno != ENOENT) {
		neb_syslog(LOG_ERR, "(port %d)port_dissociate: %m", q->fd);
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
		neb_syslog(LOG_ERR, "(epoll %d)epoll_ctl: %m", q->fd);
		ret = -1;
	} else {
		close(s->s_itimer.fd);
		s->s_itimer.fd = -1;
	}
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	struct kevent ke;
	EV_SET(&ke, s->s_itimer.ident, EVFILT_TIMER, EV_DISABLE | EV_DELETE, 0, 0, NULL);
	if (kevent(q->fd, &ke, 1, NULL, 0, NULL) == -1) {
		neb_syslog(LOG_ERR, "(kqueue %d)kevent: %m", q->fd);
		ret = -1;
	}
#elif defined(OS_SOLARIS)
	if (timer_delete(s->s_itimer.timerid) == -1) {
		neb_syslog(LOG_ERR, "(port %d)timer_delete: %m", q->fd);
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
		neb_syslog(LOG_ERR, "(epoll %d)epoll_ctl: %m", q->fd);
		ret = -1;
	} else {
		close(s->s_abstimer.fd);
		s->s_abstimer.fd = -1;
	}
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	struct kevent ke;
	EV_SET(&ke, s->s_abstimer.ident, EVFILT_TIMER, EV_DISABLE | EV_DELETE, 0, 0, NULL);
	if (kevent(q->fd, &ke, 1, NULL, 0, NULL) == -1 && errno != ENOENT) {
		neb_syslog(LOG_ERR, "(kqueue %d)kevent: %m", q->fd);
		ret = -1;
	}
#elif defined(OS_SOLARIS)
	if (timer_delete(s->s_abstimer.timerid) == -1) {
		neb_syslog(LOG_ERR, "(port %d)timer_delete: %m", q->fd);
		ret = -1;
	} else {
		s->s_abstimer.timerid = -1;
	}
#else
# error "fix me"
#endif
	return ret;
}

static void dispatch_queue_rm_pending_events(dispatch_queue_t q, dispatch_source_t s)
{
	void *s_got = NULL, *s_to_rm = s;
	if (!q->ee)
		return;
	for (int i = q->current_event; i < q->total_events; i++) {
#if defined(OS_LINUX)
		struct epoll_event *e = q->ee + i;
		s_got = e->data.ptr;
		if (s_got == s_to_rm)
			e->data.ptr = NULL;
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
		struct kevent *e = q->ee + i;
		s_got = (dispatch_source_t)e->udata;
		if (s_got == s_to_rm)
			e->udata = NULL;
#elif defined(OS_SOLARIS)
		port_event_t *e = q->ee + i;
		s_got = e->portev_user;
		if (s_got == s_to_rm)
			e->portev_user = NULL;
#else
# error "fix me"
#endif
	}
	for (int i = 0; i < q->re_add_num; i++) {
		if (q->re_add_sources[i] == s)
			q->re_add_sources[i] = NULL;
	}
}

static int dispatch_queue_rm_internal(dispatch_queue_t q, dispatch_source_t s)
{
	int ret = 0;
	if (!s->q_in_use)
		return ret;
	dispatch_queue_rm_pending_events(q, s);
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
	s->q_in_use = NULL;
	return ret;
}

void neb_dispatch_queue_rm(dispatch_queue_t q, dispatch_source_t s)
{
	int ret = 0;
	if (!s->q_in_use)
		return;
	ret = dispatch_queue_rm_internal(q, s);
	if (ret != 0)
		neb_syslog(LOG_ERR, "Error occur while removing source"); // TODO desc
	int64_t k = (int64_t)s;
	g_hash_table_remove(q->sources, &k);
}

int neb_dispatch_source_del(dispatch_source_t s)
{
	if (s->q_in_use) {
		neb_syslog(LOG_ERR, "source is currently in use");
		return -1;
	}

	free(s);
	return 0;
}

void neb_dispatch_source_set_udata(dispatch_source_t s, void *udata)
{
	s->udata = udata;
}

void *neb_dispatch_source_get_udata(dispatch_source_t s)
{
	return s->udata;
}

dispatch_queue_t neb_dispatch_source_get_queue(dispatch_source_t s)
{
	return s->q_in_use;
}

void neb_dispatch_source_set_on_remove(dispatch_source_t s, source_cb_t cb)
{
	s->on_remove = cb;
}

void neb_dispatch_source_set_readd(dispatch_source_t s, int immediatly)
{
	s->re_add_immediatly = immediatly;
}

dispatch_source_t neb_dispatch_source_new_fd_read(int fd, io_handler_t rf, io_handler_t hf)
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
	return s;
}

dispatch_source_t neb_dispatch_source_new_fd_write(int fd, io_handler_t wf, io_handler_t hf)
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
	return s;
}

dispatch_source_t neb_dispatch_source_new_itimer_sec(unsigned int ident, int64_t sec, timer_handler_t tf)
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
	return s;
}

dispatch_source_t neb_dispatch_source_new_itimer_msec(unsigned int ident, int64_t msec, timer_handler_t tf)
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
	return s;
}

dispatch_source_t neb_dispatch_source_new_abstimer(unsigned int ident, int sec_of_day, int interval_hour, timer_handler_t tf)
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
	if (s->s_itimer.timer_call)
		ret = s->s_itimer.timer_call(s->s_itimer.ident, s->udata);
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

static dispatch_cb_ret_t handle_event(dispatch_queue_t q, int i)
{
	dispatch_cb_ret_t ret = DISPATCH_CB_CONTINUE;
	dispatch_source_t s = NULL;
#if defined(OS_LINUX)
	struct epoll_event *e = q->ee + i;
	s = e->data.ptr;
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	struct kevent *e = q->ee + i;
	s = (dispatch_source_t)e->udata;
#elif defined(OS_SOLARIS)
	port_event_t *e = q->ee + i;
	s = e->portev_user;
#else
# error "fix me"
#endif

	if (!s) // source has been removed
		return ret;

	switch (s->type) {
	case DISPATCH_SOURCE_FD:
		ret = handle_source_fd(s, e);
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
		neb_dispatch_queue_rm(q, s);
		ret = DISPATCH_CB_CONTINUE;
		if (s->on_remove)
			s->on_remove(s);
		break;
	case DISPATCH_CB_READD:
		s->is_re_add = 1;
		if (s->re_add_immediatly) {
			if (neb_dispatch_queue_add(q, s) != 0) {
				neb_syslog(LOG_ERR, "Failed to readd source"); // TODO desc
				ret = DISPATCH_CB_BREAK;
			} else {
				ret = DISPATCH_CB_CONTINUE;
			}
		} else {
			q->re_add_sources[q->re_add_num++] = s;
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
		events = epoll_wait(q->fd, q->ee, q->batch_size, -1);
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
		events = kevent(q->fd, NULL, 0, q->ee, q->batch_size, NULL);
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
		uint_t nget = 1;
		if (port_getn(q->fd, q->ee, q->batch_size, &nget, NULL) == -1) {
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
		q->total_events = events;
		for (int i = 0; i < events; i++) {
			q->current_event = i;
			if (handle_event(q, i) == DISPATCH_CB_BREAK)
				goto exit_return;
		}
		for (int i = 0; i < q->re_add_num; i++) {
			dispatch_source_t re_add_s = q->re_add_sources[i];
			if (!re_add_s)
				continue;
			if (neb_dispatch_queue_add(q, re_add_s) != 0) {
				neb_syslog(LOG_ERR, "Failed to readd source"); // TODO desc
				goto exit_return;
			}
		}
		if (q->batch_call) {
			if (q->batch_call(q->udata) == DISPATCH_CB_BREAK)
				goto exit_return;
		}
	}
exit_return:
	return ret;
}
