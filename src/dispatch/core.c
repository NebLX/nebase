
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/dispatch.h>
#include <nebase/time.h>
#include <nebase/events.h>

#include "timer.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#ifdef USE_AIO_POLL
# ifdef OS_LINUX
#  include <linux/version.h>
#  if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0)
#   warning "aio poll is not available with current kernel header version"
#   undef USE_AIO_POLL
#  endif
# else
#  warning "aio poll is not available on this platform"
#  undef USE_AIO_POLL
# endif
#endif

#if defined(OS_LINUX)
# include <sys/timerfd.h>
# ifdef USE_AIO_POLL
#  include "aio_poll.h"
_Static_assert(sizeof(struct io_event) > sizeof(struct iocb *), "Size of struct io_event should be large than pointer size");
# else
#  include <sys/epoll.h>
# endif
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
# include <sys/types.h>
# include <sys/event.h>
# include <sys/time.h>
# include <string.h>
#elif defined(OS_SOLARIS)
# include <port.h>
# include <time.h>
# include <poll.h>
#endif

#include <glib.h>

enum {
	DISPATCH_SOURCE_NONE,
	DISPATCH_SOURCE_FULL_FD,
	DISPATCH_SOURCE_READ_FD,
	DISPATCH_SOURCE_ITIMER,
	DISPATCH_SOURCE_ABSTIMER,
};

struct dispatch_queue {
	struct {
#if defined(OS_LINUX)
# ifdef USE_AIO_POLL
		aio_context_t id;
		struct io_event *ee;
# else
		int fd;
		struct epoll_event *ee;
# endif
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
		int fd;
		struct kevent *ee;
#elif defined(OS_SOLARIS)
		int fd;
		port_event_t *ee;
#else
# error "fix me"
#endif
		int nevents;
		int current_event; // start from 0, so should be < nevents
	} context;

	int batch_size;

	int update_msec;
	dispatch_timer_t timer;
	get_msec_t get_msec;
	int64_t cur_msec;

	user_handler_t event_call;
	user_handler_t batch_call;

	GHashTable *sources;

	dispatch_source_t *re_add_sources;
	int re_add_num;

	uint64_t round;
	uint64_t total_events;

	void *udata;
};

struct dispatch_source_fd {
	int fd;
	int in_event;
	io_handler_t read_call;
	io_handler_t write_call;
	io_handler_t hup_call;

	struct {
#if defined(OS_LINUX)
# ifdef USE_AIO_POLL
		int skip_re_add;
# else
		int needed;
		int epoll_op;
# endif
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
		uint64_t round;
		int r_needed;
		int r_flags;
		int w_needed;
		int w_flags;
#elif defined(OS_SOLARIS)
		int skip_re_add;
#else
# error "fix me"
#endif
	} update;

#if (defined(OS_LINUX) && defined(USE_AIO_POLL)) || defined(OS_SOLARIS)
	/*
	 * If this source will be removed after running cb, set this flag to skip
	 * the real call of cancel syscall
	 */
	int skip_sys_del;
#endif
};

struct dispatch_source_itimer {
	unsigned int ident;
	timer_handler_t timer_call;
	int64_t sec;
	int64_t msec;
#if defined(OS_LINUX)
	int fd;
	struct itimerspec it;
#elif defined(OS_SOLARIS)
	timer_t timerid;
	struct itimerspec it;
#else
	int fake_id;
#endif
};

struct dispatch_source_abstimer {
	unsigned int ident;
	int sec_of_day;
	int interval_hour;
	timer_handler_t timer_call;
#if defined(OS_LINUX)
	int fd;
	struct itimerspec it;
#elif defined(OS_SOLARIS)
	timer_t timerid;
	struct itimerspec it;
#else
	int fake_id;
#endif
};

struct dispatch_source {
	int type;
	int re_add_immediatly;
	dispatch_queue_t q_in_use; // reference
	void *udata;
	source_cb_t on_remove;
#if defined(OS_LINUX)
# ifdef USE_AIO_POLL
	struct iocb ctl_event;
# else
	struct epoll_event ctl_event;
# endif
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	struct kevent ctl_event;
#elif defined(OS_SOLARIS)
	int ctl_event;
#endif
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

dispatch_queue_t neb_dispatch_queue_create(int batch_size)
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
	q->batch_size = batch_size;

	q->get_msec = neb_time_get_msec;

#if defined(OS_LINUX)
# ifdef USE_AIO_POLL
	q->context.ee = malloc(q->batch_size * sizeof(struct io_event));
# else
	q->context.ee = malloc(q->batch_size * sizeof(struct epoll_event));
# endif
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	q->context.ee = malloc(q->batch_size * sizeof(struct kevent) * 2); //double for fd event
#elif defined(OS_SOLARIS)
	q->context.ee = malloc(q->batch_size * sizeof(port_event_t));
#else
# error "fix me"
#endif
	if (!q->context.ee) {
		neb_syslog(LOG_ERR, "malloc: %m");
		g_hash_table_destroy(q->sources);
		free(q);
		return NULL;
	}

	q->re_add_sources = calloc(q->batch_size, sizeof(dispatch_source_t));
	if (!q->re_add_sources) {
		neb_syslog(LOG_ERR, "calloc: %m");
		free(q->context.ee);
		g_hash_table_destroy(q->sources);
		free(q);
		return NULL;
	}
	q->re_add_num = 0;

#if defined(OS_LINUX)
# ifdef USE_AIO_POLL
	if (neb_aio_poll_create(q->batch_size, &q->context.id) == -1) {
		neb_syslog(LOG_ERR, "aio_poll_create: %m");
# else
	q->context.fd = epoll_create1(EPOLL_CLOEXEC);
	if (q->context.fd == -1) {
		neb_syslog(LOG_ERR, "epoll_create1: %m");
# endif
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	q->context.fd = kqueue();
	if (q->context.fd == -1) {
		neb_syslog(LOG_ERR, "kqueue: %m");
#elif defined(OS_SOLARIS)
	q->context.fd = port_create();
	if (q->context.fd == -1) {
		neb_syslog(LOG_ERR, "port_create: %m");
#else
# error "fix me"
#endif
		free(q->re_add_sources);
		free(q->context.ee);
		g_hash_table_destroy(q->sources);
		free(q);
		return NULL;
	}

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
	if (q->context.ee)
		free(q->context.ee);
	if (q->re_add_sources)
		free(q->re_add_sources);
#ifdef USE_AIO_POLL
	if (q->context.id)
		neb_aio_poll_destroy(q->context.id);
#else
	if (q->context.fd >= 0)
		close(q->context.fd);
#endif
	free(q);
}

void neb_dispatch_queue_set_timer(dispatch_queue_t q, dispatch_timer_t t)
{
	q->timer = t;
}

void neb_dispatch_queue_set_get_msec(dispatch_queue_t q, get_msec_t fn)
{
	q->get_msec = fn;
}

int64_t neb_dispatch_queue_get_abs_timeout(dispatch_queue_t q, int msec)
{
	return q->cur_msec + msec;
}

void neb_dispatch_queue_update_cur_msec(dispatch_queue_t q)
{
	q->cur_msec = q->get_msec();
}

void neb_dispatch_queue_set_event_handler(dispatch_queue_t q, user_handler_t ef)
{
	q->event_call = ef;
}

void neb_dispatch_queue_set_batch_handler(dispatch_queue_t q, user_handler_t bf)
{
	q->batch_call = bf;
}

void neb_dispatch_queue_set_user_data(dispatch_queue_t q, void *udata)
{
	q->udata = udata;
}

static void get_events_for_fd(dispatch_source_t s)
{
#if defined(OS_LINUX)
# ifdef USE_AIO_POLL
	s->ctl_event.aio_lio_opcode = IOCB_CMD_POLL;
	s->ctl_event.aio_fildes = s->s_fd.fd;
	s->ctl_event.aio_data = (uint64_t)s;
	s->ctl_event.aio_buf = POLLHUP;
	if (s->s_fd.read_call)
		s->ctl_event.aio_buf |= POLLIN;
	if (s->s_fd.write_call)
		s->ctl_event.aio_buf |= POLLOUT;
# else
	s->ctl_event.data.ptr = s;
	s->ctl_event.events = EPOLLHUP;
	if (s->s_fd.read_call)
		s->ctl_event.events |= EPOLLIN;
	if (s->s_fd.write_call)
		s->ctl_event.events |= EPOLLOUT;
# endif
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	// EV_EOF is always set
	EV_SET(&s->ctl_event, s->s_fd.fd, 0, EV_ADD | EV_ENABLE, 0, 0, s);
#elif defined(OS_SOLARIS)
	s->ctl_event = POLLHUP;
	if (s->s_fd.read_call)
		s->ctl_event |= POLLIN;
	if (s->s_fd.write_call)
		s->ctl_event |= POLLOUT;
#else
# error "fix me"
#endif
}

static int add_source_fd(dispatch_queue_t q, dispatch_source_t s)
{
	if (!s->s_fd.read_call && !s->s_fd.write_call)
		return 0;
#if defined(OS_LINUX)
# ifdef USE_AIO_POLL
	// re add ok
	struct iocb *iocbp = &s->ctl_event;
	if (neb_aio_poll_submit(q->context.id, 1, &iocbp) == -1) {
		neb_syslog(LOG_ERR, "(aio %lu)aio_poll_submit: %m", q->context.id);
		return -1;
	}
	s->s_fd.skip_sys_del = 0; // clear this flag if user readd a removed one
# else
	// no re add
	if (epoll_ctl(q->context.fd, EPOLL_CTL_ADD, s->s_fd.fd, &s->ctl_event) == -1) {
		neb_syslog(LOG_ERR, "(epoll %d)epoll_ctl(ADD): %m", q->context.fd);
		return -1;
	}
# endif
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	if (s->s_fd.read_call) {
		// no re add
		s->ctl_event.filter = EVFILT_READ;
		s->ctl_event.flags = EV_ADD | EV_ENABLE;
		if (kevent(q->context.fd, &s->ctl_event, 1, NULL, 0, NULL) == -1) { // updated if dup
			neb_syslog(LOG_ERR, "(kqueue %d)kevent: %m", q->context.fd);
			return -1;
		}
	}
	if (s->s_fd.write_call) {
		// no re add
		s->ctl_event.filter = EVFILT_WRITE;
		s->ctl_event.flags = EV_ADD | EV_ENABLE;
		if (kevent(q->context.fd, &s->ctl_event, 1, NULL, 0, NULL) == -1) { // updated if dup
			neb_syslog(LOG_ERR, "(kqueue %d)kevent: %m", q->context.fd);
			return -1;
		}
	}
#elif defined(OS_SOLARIS)
	// re add ok
	if (port_associate(q->context.fd, PORT_SOURCE_FD, s->s_fd.fd, s->ctl_event, s) == -1) {
		neb_syslog(LOG_ERR, "(port %d)port_associate: %m", q->context.fd);
		return -1;
	}
	s->s_fd.skip_sys_del = 0; // clear this flag if user readd a removed one
#else
# error "fix me"
#endif
	return 0;
}

static int update_source_fd(dispatch_queue_t q, dispatch_source_t s, io_handler_t rf, io_handler_t wf)
{
	io_handler_t old_rf = s->s_fd.read_call;
	io_handler_t old_wf = s->s_fd.write_call;
	s->s_fd.read_call = rf;
	s->s_fd.write_call = wf;
	neb_syslog(LOG_DEBUG, "Changing IO CB: read: %p -> %p, write: %p -> %p, queue: %p",
	           old_rf, rf, old_wf, wf, q);

#if defined(OS_LINUX)
	if ((old_rf == NULL) == (rf == NULL) &&
	    (old_wf == NULL) == (wf == NULL))
		return 0; // no changes
# ifdef USE_AIO_POLL
	if (s->s_fd.in_event) {
		if (!rf && !wf) {
			s->s_fd.update.skip_re_add = 1;
			return 0;
		}
		get_events_for_fd(s);
		s->s_fd.update.skip_re_add = 0;
	} else {
		struct iocb *iocbp = &s->ctl_event;
		if (!rf && !wf) {
			if (neb_aio_poll_cancel(q->context.id, iocbp, NULL) == -1 && errno != ENOENT) {
				neb_syslog(LOG_ERR, "(aio %lu)aio_poll_cancel: %m", q->context.id);
				return -1;
			}
		} else {
			get_events_for_fd(s);
			if (neb_aio_poll_submit(q->context.id, 1, &iocbp) == -1) {
				neb_syslog(LOG_ERR, "(aio %lu)aio_poll_submit: %m", q->context.id);
				return -1;
			}
		}
	}
# else
	if (rf || wf) {
		get_events_for_fd(s);
		if (old_rf || old_wf)
			s->s_fd.update.epoll_op = EPOLL_CTL_MOD;
		else
			s->s_fd.update.epoll_op = EPOLL_CTL_ADD;
	} else {
		s->s_fd.update.epoll_op = EPOLL_CTL_DEL;
	}
	if (s->s_fd.in_event && !s->re_add_immediatly) {
		s->s_fd.update.needed = 1;
		return 0;
	}
	// re add here as add_source_fd does not support re-add of epoll_event
	s->s_fd.update.needed = 0;
	if (epoll_ctl(q->context.fd, s->s_fd.update.epoll_op, s->s_fd.fd, &s->ctl_event) == -1) {
		if (s->s_fd.update.epoll_op == EPOLL_CTL_DEL && errno == ENOENT)
			return 0;
		neb_syslog(LOG_ERR, "epoll_ctl(op:%d): %m", s->s_fd.update.epoll_op);
		return -1;
	}
# endif
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	struct kevent *re = q->context.ee + q->context.current_event;
	switch (re->filter) {
	case EVFILT_READ:
		if ((old_rf == NULL) == (rf == NULL))
			return 0; // no change
		s->s_fd.update.r_needed = 1;
		if (rf)
			s->s_fd.update.r_flags = EV_ADD | EV_ENABLE;
		else
			s->s_fd.update.r_flags = EV_DISABLE | EV_DELETE;
		break;
	case EVFILT_WRITE:
		if ((old_wf == NULL) == (wf == NULL))
			return 0; // no change
		s->s_fd.update.w_needed = 1;
		if (wf)
			s->s_fd.update.w_flags = EV_ADD | EV_ENABLE;
		else
			s->s_fd.update.w_flags = EV_DISABLE | EV_DELETE;
		break;
	default:
		return 0;
		break;
	}
	if (s->s_fd.in_event && !s->re_add_immediatly)
		return 0;
	// re add here as add_source_fd does not support re-add of kevent
	if (s->s_fd.update.r_needed) {
		s->ctl_event.filter = EVFILT_READ;
		s->ctl_event.flags = s->s_fd.update.r_flags;
		if (kevent(q->context.fd, &s->ctl_event, 1, NULL, 0, NULL) == -1) { // updated if dup
			neb_syslog(LOG_ERR, "(kqueue %d)kevent(flags:%d): %m", q->context.fd, s->s_fd.update.r_flags);
			return -1;
		}
		s->s_fd.update.r_needed = 0;
	}
	if (s->s_fd.update.w_needed) {
		s->ctl_event.filter = EVFILT_WRITE;
		s->ctl_event.flags = s->s_fd.update.w_flags;
		if (kevent(q->context.fd, &s->ctl_event, 1, NULL, 0, NULL) == -1) { // updated if dup
			neb_syslog(LOG_ERR, "(kqueue %d)kevent(flags:%d): %m", q->context.fd, s->s_fd.update.w_flags);
			return -1;
		}
		s->s_fd.update.w_needed = 0;
	}
#elif defined(OS_SOLARIS)
	if ((old_rf == NULL) == (rf == NULL) &&
	    (old_wf == NULL) == (wf == NULL))
		return 0; // no changes
	if (s->s_fd.in_event) {
		if (!rf && !wf) {
			s->s_fd.update.skip_re_add = 1;
			return 0;
		}
		get_events_for_fd(s);
		s->s_fd.update.skip_re_add = 0;
	} else {
		if (!rf && !wf) {
			if (port_dissociate(q->context.fd, PORT_SOURCE_FD, s->s_fd.fd) == -1 && errno != ENOENT) {
				neb_syslog(LOG_ERR, "(port %d)port_dissociate: %m", q->context.fd);
				return -1;
			}
		} else {
			get_events_for_fd(s);
			if (port_associate(q->context.fd, PORT_SOURCE_FD, s->s_fd.fd, s->ctl_event, s) == -1) {
				neb_syslog(LOG_ERR, "(port %d)port_associate: %m", q->context.fd);
				return -1;
			}
		}
	}
#else
# error "fix me"
#endif
	return 0;
}

static int create_itimer(dispatch_source_t s)
{
#if defined(OS_LINUX)
	int64_t sec = s->s_itimer.sec;
	int64_t nsec = (sec) ? 0 : (int64_t)s->s_itimer.msec * 1000000;
	struct itimerspec *it = &s->s_itimer.it;
	it->it_value.tv_sec = sec;
	it->it_value.tv_nsec = nsec;
	it->it_interval.tv_sec = sec;
	it->it_interval.tv_nsec = nsec;
	if (s->s_itimer.fd == -1) {
		s->s_itimer.fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
		if (s->s_itimer.fd == -1) {
			neb_syslog(LOG_ERR, "timerfd_create: %m");
			return -1;
		}
	}
	// arm the timer
	if (timerfd_settime(s->s_itimer.fd, 0, it, NULL) == -1) {
		neb_syslog(LOG_ERR, "timerfd_settime: %m");
		return -1;
	}
	// set events
# ifdef USE_AIO_POLL
	s->ctl_event.aio_lio_opcode = IOCB_CMD_POLL;
	s->ctl_event.aio_fildes = s->s_itimer.fd;
	s->ctl_event.aio_data = (uint64_t)s;
	s->ctl_event.aio_buf = POLLIN;
# else
	s->ctl_event.data.ptr = s;
	s->ctl_event.events = EPOLLIN;
# endif
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
	EV_SET(&s->ctl_event, s->s_itimer.ident, EVFILT_TIMER, EV_ADD | EV_ENABLE, fflags, data, s);
#elif defined(OS_SOLARIS)
	int64_t sec = s->s_itimer.sec;
	int64_t nsec = (sec) ? 0 : (int64_t)s->s_itimer.msec * 1000000;
	struct itimerspec *it = &s->s_itimer.it;
	it->it_value.tv_sec = sec;
	it->it_value.tv_nsec = nsec;
	it->it_interval.tv_sec = sec;
	it->it_interval.tv_nsec = nsec;
	if (s->s_itimer.timerid != -1 &&
	    timer_settime(s->s_itimer.timerid, 0, it, NULL) == -1) {
		neb_syslog(LOG_ERR, "timer_settime: %m");
		return -1;
	}
#else
# error "fix me"
#endif
	return 0;
}

static int add_source_itimer(dispatch_queue_t q, dispatch_source_t s)
{
#if defined(OS_LINUX)
# ifdef USE_AIO_POLL
	// re add ok
	struct iocb *iocbp = &s->ctl_event;
	if (neb_aio_poll_submit(q->context.id, 1, &iocbp) == -1) {
		neb_syslog(LOG_ERR, "(aio %lu)aio_poll_submit: %m", q->context.id);
		return -1;
	}
# else
	// no re add
	if (epoll_ctl(q->context.fd, EPOLL_CTL_ADD, s->s_itimer.fd, &s->ctl_event) == -1) {
		neb_syslog(LOG_ERR, "(epoll %d)epoll_ctl(ADD): %m", q->context.fd);
		return -1;
	}
# endif
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	// no re add
	if (kevent(q->context.fd, &s->ctl_event, 1, NULL, 0, NULL) == -1) {
		neb_syslog(LOG_ERR, "(kqueue %d)kevent: %m", q->context.fd);
		return -1;
	}
#elif defined(OS_SOLARIS)
	// no re add
	if (s->s_itimer.timerid == -1) {
		// create the timer and associate with the port
		port_notify_t pn = {
			.portnfy_port = q->context.fd,
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
	if (timer_settime(s->s_itimer.timerid, 0, &s->s_itimer.it, NULL) == -1) {
		neb_syslog(LOG_ERR, "timer_settime: %m");
		return -1;
	}
#else
# error "fix me"
#endif
	return 0;
}

static int create_or_update_abstimer(dispatch_source_t s)
{
	// update ok
	time_t abs_ts;
	int delta_sec;
	if (neb_daytime_abs_nearest(s->s_abstimer.sec_of_day, &abs_ts, &delta_sec) != 0) {
		neb_syslog(LOG_ERR, "Failed to get next abs time for sec_of_day %d", s->s_abstimer.sec_of_day);
		return -1;
	}
#if defined(OS_LINUX)
	time_t interval_sec = (time_t)s->s_abstimer.interval_hour * 3600;
	struct itimerspec *it = &s->s_abstimer.it;
	it->it_value.tv_sec = abs_ts;
	it->it_value.tv_nsec = 0;
	it->it_interval.tv_sec = interval_sec;
	it->it_interval.tv_nsec = 0;
	if (s->s_abstimer.fd == -1) {
		s->s_abstimer.fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
		if (s->s_abstimer.fd == -1) {
			neb_syslog(LOG_ERR, "timerfd_create: %m");
			return -1;
		}
	}
	// arm the timer
	if (timerfd_settime(s->s_abstimer.fd, TFD_TIMER_ABSTIME, it, NULL) == -1) {
		neb_syslog(LOG_ERR, "timerfd_settime: %m");
		return -1;
	}
# ifdef USE_AIO_POLL
	s->ctl_event.aio_lio_opcode = IOCB_CMD_POLL;
	s->ctl_event.aio_fildes = s->s_abstimer.fd;
	s->ctl_event.aio_data = (uint64_t)s;
	s->ctl_event.aio_buf = POLLIN;
# else
	s->ctl_event.data.ptr = s;
	s->ctl_event.events = EPOLLIN;
# endif
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
	EV_SET(&s->ctl_event, s->s_abstimer.ident, EVFILT_TIMER, EV_ADD | EV_ENABLE | EV_ONESHOT, fflags, data, s);
#elif defined(OS_SOLARIS)
	time_t interval_sec = (time_t)s->s_abstimer.interval_hour * 3600;
	struct itimerspec *it = &s->s_abstimer.it;
	it->it_value.tv_sec = abs_ts;
	it->it_value.tv_nsec = 0;
	it->it_interval.tv_sec = interval_sec;
	it->it_interval.tv_nsec = 0;
	if (s->s_abstimer.timerid != -1 &&
	    timer_settime(s->s_abstimer.timerid, TIMER_ABSTIME, it, NULL) == -1) {
		neb_syslog(LOG_ERR, "timer_settime: %m");
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
#if defined(OS_LINUX)
# ifdef USE_AIO_POLL
	// re add ok
	struct iocb *iocbp = &s->ctl_event;
	if (neb_aio_poll_submit(q->context.id, 1, &iocbp) == -1) {
		neb_syslog(LOG_ERR, "(aio %lu)aio_poll_submit: %m", q->context.id);
		return -1;
	}
# else
	// no re add
	if (epoll_ctl(q->context.fd, EPOLL_CTL_ADD, s->s_abstimer.fd, &s->ctl_event) == -1) {
		neb_syslog(LOG_ERR, "(epoll %d)epoll_ctl(ADD): %m", q->context.fd);
		return -1;
	}
# endif
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	// re add ok
	if (kevent(q->context.fd, &s->ctl_event, 1, NULL, 0, NULL) == -1) {
		neb_syslog(LOG_ERR, "(kqueue %d)kevent: %m", q->context.fd);
		return -1;
	}
#elif defined(OS_SOLARIS)
	// no re add
	if (s->s_abstimer.timerid == -1) {
		// create the timer and associate with the port
		port_notify_t pn = {
			.portnfy_port = q->context.fd,
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
	if (timer_settime(s->s_abstimer.timerid, TIMER_ABSTIME, &s->s_abstimer.it, NULL) == -1) {
		neb_syslog(LOG_ERR, "timer_settime: %m");
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
	if (s->q_in_use)
		return ret;
	switch (s->type) {
	case DISPATCH_SOURCE_FULL_FD:
	case DISPATCH_SOURCE_READ_FD:
		get_events_for_fd(s);
		ret = add_source_fd(q, s);
		break;
	case DISPATCH_SOURCE_ITIMER:
		if (create_itimer(s) != 0) {
			neb_syslog(LOG_ERR, "Failed to create itimer");
			ret = -1;
		} else {
			ret = add_source_itimer(q, s);
		}
		break;
	case DISPATCH_SOURCE_ABSTIMER:
		if (create_or_update_abstimer(s) != 0) {
			neb_syslog(LOG_ERR, "Failed to create abstimer");
			ret = -1;
		} else {
			ret = add_source_abstimer(q, s);
		}
		break;
	case DISPATCH_SOURCE_NONE: /* fall through */
	default:
		neb_syslog(LOG_ERR, "Invalid source type %d", s->type);
		ret = -1;
		break;
	}
	if (ret == 0) {
		s->q_in_use = q;
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
	return ret;
}

static int dispatch_queue_readd(dispatch_queue_t q, dispatch_source_t s)
{
	switch (s->type) {
	case DISPATCH_SOURCE_FULL_FD:
	case DISPATCH_SOURCE_READ_FD:
#if (defined(OS_LINUX) && defined(USE_AIO_POLL)) || defined(OS_SOLARIS)
		return add_source_fd(q, s);
#endif
		break;
	case DISPATCH_SOURCE_ITIMER:
#if defined(OS_LINUX) && defined(USE_AIO_POLL)
		return add_source_itimer(q, s);
#endif
		break;
	case DISPATCH_SOURCE_ABSTIMER:
		if (create_or_update_abstimer(s) != 0) {
			neb_syslog(LOG_ERR, "Failed to update abstimer"); // TODO desc
			return -1;
		} else {
#if (defined(OS_LINUX) && defined(USE_AIO_POLL)) ||\
     defined(OSTYPE_BSD) || defined(OS_DARWIN)
			return add_source_abstimer(q, s);
#endif
		}
		break;
	default:
		neb_syslog(LOG_ERR, "Unsupported readd source type %d to queue %p", s->type, q);
		return -1;
		break;
	}
	return 0;
}

static int dispatch_queue_readd_batch(dispatch_queue_t q)
{
#if defined(OS_LINUX)
# ifdef USE_AIO_POLL
	struct iocb **iocbpp = (struct iocb **)q->context.ee; // ee is large enough
	int changes = 0;
# else
	/* no batch for epoll_ctl */
# endif
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	struct kevent *events = q->context.ee;
	int changes = 0;
#elif defined(OS_SOLARIS)
	/* no batch for port_associate */
#else
# error "fix me"
#endif
	int re_add_num = q->re_add_num;
	q->re_add_num = 0;
	for (int i = 0; i < re_add_num; i++) {
		dispatch_source_t s = q->re_add_sources[i];
		if (!s)
			continue;
		switch (s->type) {
		case DISPATCH_SOURCE_FULL_FD:
		case DISPATCH_SOURCE_READ_FD:
#if defined(OS_LINUX)
# ifdef USE_AIO_POLL
			*(iocbpp + changes++) = &s->ctl_event;
# else
			if (epoll_ctl(q->context.fd, s->s_fd.update.epoll_op, s->s_fd.fd, &s->ctl_event) == -1) {
				if (s->s_fd.update.epoll_op == EPOLL_CTL_DEL && errno == ENOENT)
					return 0;
				neb_syslog(LOG_ERR, "epoll_ctl(op:%d): %m", s->s_fd.update.epoll_op);
				return -1;
			}
# endif
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
			if (s->s_fd.update.r_needed) {
				s->ctl_event.filter = EVFILT_READ;
				s->ctl_event.flags = s->s_fd.update.r_flags;
				memcpy(events + changes++,  &s->ctl_event, sizeof(kevent));
				s->s_fd.update.r_needed = 0;
			}
			if (s->s_fd.update.w_needed) {
				s->ctl_event.filter = EVFILT_WRITE;
				s->ctl_event.flags = s->s_fd.update.w_flags;
				memcpy(events + changes++,  &s->ctl_event, sizeof(kevent));
				s->s_fd.update.w_needed = 0;
			}
#elif defined(OS_SOLARIS)
			if (port_associate(q->context.fd, PORT_SOURCE_FD, s->s_fd.fd, s->ctl_event, s) == -1) {
				neb_syslog(LOG_ERR, "(port %d)port_associate: %m", q->context.fd);
				return -1;
			}
#endif
			break;
		case DISPATCH_SOURCE_ITIMER:
#if defined(OS_LINUX) && defined(USE_AIO_POLL)
				*(iocbpp + changes++) = &s->ctl_event;
#endif
			break;
		case DISPATCH_SOURCE_ABSTIMER:
			if (create_or_update_abstimer(s) != 0) {
				neb_syslog(LOG_ERR, "Failed to update abstimer"); // TODO desc
				return -1;
			} else {
#if defined(OS_LINUX) && defined(USE_AIO_POLL)
				*(iocbpp + changes++) = &s->ctl_event;
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
				memcpy(events + changes++,  &s->ctl_event, sizeof(kevent));
#endif
			}
			break;
		default:
			neb_syslog(LOG_ERR, "Unsupported readd source type %d", s->type);
			return -1;
			break;
		}
	}

#if defined(OS_LINUX)
# ifdef USE_AIO_POLL
	if (changes && neb_aio_poll_submit(q->context.id, changes, iocbpp) == -1) {
		neb_syslog(LOG_ERR, "(aio %lu)aio_poll_submit: %m", q->context.id);
		return -1;
	}
# else
	/* no batch for epoll_ctl */
# endif
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	if (changes && kevent(q->context.fd, events, changes, NULL, 0, NULL) == -1) {
		neb_syslog(LOG_ERR, "(kqueue %d)kevent: %m", q->context.fd);
		return -1;
	}
#endif
	return 0;
}

static int rm_source_fd(dispatch_queue_t q, dispatch_source_t s)
{
	// no resource validation as we use in_use as guard
#if defined(OS_LINUX)
# ifdef USE_AIO_POLL
	if (s->s_fd.skip_sys_del)
		return 0;
	if (neb_aio_poll_cancel(q->context.id, &s->ctl_event, NULL) == -1 && errno != ENOENT) {
		neb_syslog(LOG_ERR, "(aio %lu)aio_poll_cancel: %m", q->context.id);
		return -1;
	}

# else
	if (epoll_ctl(q->context.fd, EPOLL_CTL_DEL, s->s_fd.fd, NULL) == -1 && errno != ENOENT) {
		neb_syslog(LOG_ERR, "(epoll %d)epoll_ctl: %m", q->context.fd);
		return -1;
	}
# endif
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	s->ctl_event.flags = EV_DISABLE | EV_DELETE;
	if (s->s_fd.read_call) {
		s->ctl_event.filter = EVFILT_READ;
		if (kevent(q->context.fd, &s->ctl_event, 1, NULL, 0, NULL) == -1 && errno != ENOENT) {
			neb_syslog(LOG_ERR, "(kqueue %d)kevent: %m", q->context.fd);
			return -1;
		}
	}
	if (s->s_fd.write_call) {
		s->ctl_event.filter = EVFILT_WRITE;
		if (kevent(q->context.fd, &s->ctl_event, 1, NULL, 0, NULL) == -1 && errno != ENOENT) {
			neb_syslog(LOG_ERR, "(kqueue %d)kevent: %m", q->context.fd);
			return -1;
		}
	}
#elif defined(OS_SOLARIS)
	if (s->s_fd.skip_sys_del)
		return 0;
	if (port_dissociate(q->context.fd, PORT_SOURCE_FD, s->s_fd.fd) == -1 && errno != ENOENT) {
		neb_syslog(LOG_ERR, "(port %d)port_dissociate: %m", q->context.fd);
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
# ifdef USE_AIO_POLL
	if (neb_aio_poll_cancel(q->context.id, &s->ctl_event, NULL) == -1 && errno != ENOENT) {
		neb_syslog(LOG_ERR, "(aio %lu)aio_poll_cancel: %m", q->context.id);
		ret = -1;
# else
	if (epoll_ctl(q->context.fd, EPOLL_CTL_DEL, s->s_itimer.fd, NULL) == -1 && errno != ENOENT) {
		neb_syslog(LOG_ERR, "(epoll %d)epoll_ctl: %m", q->context.fd);
		ret = -1;
# endif
	} else {
		close(s->s_itimer.fd);
		s->s_itimer.fd = -1;
	}
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	struct kevent ke;
	EV_SET(&ke, s->s_itimer.ident, EVFILT_TIMER, EV_DISABLE | EV_DELETE, 0, 0, NULL);
	if (kevent(q->context.fd, &ke, 1, NULL, 0, NULL) == -1 && errno != ENOENT) {
		neb_syslog(LOG_ERR, "(kqueue %d)kevent: %m", q->context.fd);
		ret = -1;
	}
#elif defined(OS_SOLARIS)
	if (timer_delete(s->s_itimer.timerid) == -1) {
		neb_syslog(LOG_ERR, "(port %d)timer_delete: %m", q->context.fd);
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
# ifdef USE_AIO_POLL
	if (neb_aio_poll_cancel(q->context.id, &s->ctl_event, NULL) == -1 && errno != ENOENT) {
		neb_syslog(LOG_ERR, "(aio %lu)aio_poll_cancel: %m", q->context.id);
		ret = -1;
# else
	if (epoll_ctl(q->context.fd, EPOLL_CTL_DEL, s->s_abstimer.fd, NULL) == -1 && errno != ENOENT) {
		neb_syslog(LOG_ERR, "(epoll %d)epoll_ctl: %m", q->context.fd);
		ret = -1;
# endif
	} else {
		close(s->s_abstimer.fd);
		s->s_abstimer.fd = -1;
	}
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	struct kevent ke;
	EV_SET(&ke, s->s_abstimer.ident, EVFILT_TIMER, EV_DISABLE | EV_DELETE, 0, 0, NULL);
	if (kevent(q->context.fd, &ke, 1, NULL, 0, NULL) == -1 && errno != ENOENT) {
		neb_syslog(LOG_ERR, "(kqueue %d)kevent: %m", q->context.fd);
		ret = -1;
	}
#elif defined(OS_SOLARIS)
	if (timer_delete(s->s_abstimer.timerid) == -1) {
		neb_syslog(LOG_ERR, "(port %d)timer_delete: %m", q->context.fd);
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
	if (!q->context.ee)
		return;
	for (int i = q->context.current_event; i < q->context.nevents; i++) {
#if defined(OS_LINUX)
# ifdef USE_AIO_POLL
		struct io_event *e = q->context.ee + i;
		s_got = (void *)e->data;
		if (s_got == s_to_rm)
			e->data = 0;
# else
		struct epoll_event *e = q->context.ee + i;
		s_got = e->data.ptr;
		if (s_got == s_to_rm)
			e->data.ptr = NULL;
# endif
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
		struct kevent *e = q->context.ee + i;
		s_got = (dispatch_source_t)e->udata;
		if (s_got == s_to_rm)
# if defined(OS_NETBSD)
			e->udata = (intptr_t)NULL;
# else
			e->udata = NULL;
# endif
#elif defined(OS_SOLARIS)
		port_event_t *e = q->context.ee + i;
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
	case DISPATCH_SOURCE_FULL_FD:
	case DISPATCH_SOURCE_READ_FD:
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
		neb_syslog(LOG_ERR, "Invalid source type %d", s->type);
		ret = -1;
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

static void clear_source_itimer(dispatch_source_t s)
{
#if defined(OS_LINUX)
	if (s->s_itimer.fd != -1) {
		close(s->s_itimer.fd);
		s->s_itimer.fd = -1;
	}
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	s->s_itimer.fake_id = 0;
#elif defined(OS_SOLARIS)
	if (s->s_itimer.timerid != -1) {
		timer_delete(s->s_itimer.timerid);
		s->s_itimer.timerid = -1;
	}
#else
# error "fix me"
#endif
}

static void clear_source_abstimer(dispatch_source_t s)
{
#if defined(OS_LINUX)
	if (s->s_abstimer.fd != -1) {
		close(s->s_abstimer.fd);
		s->s_abstimer.fd = -1;
	}
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	s->s_abstimer.fake_id = 0;
#elif defined(OS_SOLARIS)
	if (s->s_abstimer.timerid != -1) {
		timer_delete(s->s_abstimer.timerid);
		s->s_abstimer.timerid = -1;
	}
#else
# error "fix me"
#endif
}

int neb_dispatch_source_del(dispatch_source_t s)
{
	if (s->q_in_use) {
		neb_syslog(LOG_ERR, "source is currently in use");
		return -1;
	}

	switch (s->type) {
	case DISPATCH_SOURCE_FULL_FD:
	case DISPATCH_SOURCE_READ_FD:
		break;
	case DISPATCH_SOURCE_ITIMER:
		clear_source_itimer(s);
		break;
	case DISPATCH_SOURCE_ABSTIMER:
		clear_source_abstimer(s);
		break;
	default:
		break;
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

dispatch_source_t neb_dispatch_source_new_fd(int fd, io_handler_t hf)
{
	struct dispatch_source *s = calloc(1, sizeof(struct dispatch_source));
	if (!s) {
		neb_syslog(LOG_ERR, "calloc: %m");
		return NULL;
	}
	s->type = DISPATCH_SOURCE_FULL_FD;
	s->s_fd.fd = fd;
	s->s_fd.hup_call = hf;
	return s;
}

dispatch_source_t neb_dispatch_source_new_read_fd(int fd, io_handler_t rf, io_handler_t hf)
{
	struct dispatch_source *s = calloc(1, sizeof(struct dispatch_source));
	if (!s) {
		neb_syslog(LOG_ERR, "calloc: %m");
		return NULL;
	}
	s->type = DISPATCH_SOURCE_READ_FD;
	s->s_fd.fd = fd;
	s->s_fd.hup_call = hf;
	s->s_fd.read_call = rf;
	s->s_fd.write_call = NULL;
	return s;
}

int neb_dispatch_source_fd_set_io_cb(dispatch_source_t s, io_handler_t rf, io_handler_t wf)
{
	switch (s->type) {
	case DISPATCH_SOURCE_FULL_FD:
		if (s->q_in_use) {
			if (update_source_fd(s->q_in_use, s, rf, wf) != 0) {
				neb_syslog(LOG_ERR, "Failed to update io cb for running fd source");
				return -1;
			}
			return 0;
		} else {
			s->s_fd.read_call = rf;
			s->s_fd.write_call = wf;
			return 0;
		}
		break;
	case DISPATCH_SOURCE_READ_FD:
		if (!rf) {
			neb_syslog(LOG_CRIT, "it is not allowed to clear rf cb for read_fd source");
		} else {
			s->s_fd.read_call = rf;
			return 0;
		}
		break;
	default:
		neb_syslog(LOG_CRIT, "Failed to set io cb: invalid source type");
		break;
	}
	return -1;
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
#if defined(OS_LINUX)
	s->s_itimer.fd = -1;
#elif defined(OS_SOLARIS)
	s->s_itimer.timerid = -1;
#endif
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
# ifdef USE_AIO_POLL
	struct io_event *e = event;
	ehup = e->res & POLLHUP;
	eread = e->res & POLLIN;
	ewrite = e->res & POLLOUT;
# else
	struct epoll_event *e = event;
	ehup = e->events & EPOLLHUP;
	eread = e->events & EPOLLIN;
	ewrite = e->events & EPOLLOUT;
# endif
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
	s->s_fd.in_event = 1;
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
#if defined(OS_LINUX)
# ifdef USE_AIO_POLL
	if (ret == DISPATCH_CB_CONTINUE) {
		if (s->s_fd.update.skip_re_add)
			s->s_fd.update.skip_re_add = 0;
		else
			ret = DISPATCH_CB_READD;
	} else {
		s->s_fd.skip_sys_del = 1;
	}
# else
	if (s->s_fd.update.needed) {
		s->s_fd.update.needed = 0;
		ret = DISPATCH_CB_READD;
	}
# endif
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	if ((s->s_fd.update.r_needed || s->s_fd.update.w_needed) &&
	    s->s_fd.update.round != s->q_in_use->round) // only add for once
		ret = DISPATCH_CB_READD;
	s->s_fd.update.round = s->q_in_use->round;
#elif defined(OS_SOLARIS)
	if (ret == DISPATCH_CB_CONTINUE) {
		if (s->s_fd.update.skip_re_add)
			s->s_fd.update.skip_re_add = 0;
		else
			ret = DISPATCH_CB_READD;
	}
#else
# error "fix me"
#endif
	s->s_fd.in_event = 0;
	return ret;
}

static dispatch_cb_ret_t handle_source_read_fd(dispatch_source_t s, void *event)
{
	dispatch_cb_ret_t ret = DISPATCH_CB_CONTINUE;
	int eread = 0, ehup = 0;
#if defined(OS_LINUX)
# ifdef USE_AIO_POLL
	struct io_event *e = event;
	ehup = e->res & POLLHUP;
	eread = e->res & POLLIN;
# else
	struct epoll_event *e = event;
	ehup = e->events & EPOLLHUP;
	eread = e->events & EPOLLIN;
# endif
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	struct kevent *e = event;
	ehup = e->flags & EV_EOF;
	eread = (e->filter == EVFILT_READ);
#elif defined(OS_SOLARIS)
	port_event_t *e = event;
	ehup = e->portev_events & POLLHUP;
	eread = e->portev_events & POLLIN;
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
exit_return:
#if (defined(OS_LINUX) && defined(USE_AIO_POLL)) || defined(OS_SOLARIS)
	if (ret == DISPATCH_CB_CONTINUE)
		ret = DISPATCH_CB_READD;
	else
		s->s_fd.skip_sys_del = 1;
#endif
	return ret;
}

static int timer_get_overrun(void *event)
{
#if defined(OS_LINUX)
# ifdef USE_AIO_POLL
	const struct io_event *e = event;
	const struct iocb *iocb = (struct iocb *)e->obj;
	int timer_fd = iocb->aio_fildes;
# else
	struct epoll_event *e = event;
	int timer_fd = e->data.fd;
# endif
	uint64_t overrun = 0;
	if (read(timer_fd, &overrun, sizeof(overrun)) == -1) {
		neb_syslog(LOG_ERR, "read: %m");
		return -1;
	}
	return overrun;
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	struct kevent *e = event;
	return e->data;
#elif defined(OS_SOLARIS)
	port_event_t *e = event;
	int overrun = timer_getoverrun((timer_t)e->portev_object);
	if (overrun == -1)
		neb_syslog(LOG_ERR, "timer_getoverrun: %m");
	return overrun;
#else
# error "fix me"
#endif
}

static dispatch_cb_ret_t handle_source_itimer(dispatch_source_t s, void *event)
{
	int ret = DISPATCH_CB_CONTINUE;
	int overrun = timer_get_overrun(event);
	if (overrun < 0) {
		neb_syslog(LOG_ERR, "Failed to get itimer overrun");
		return DISPATCH_CB_BREAK;
	}
	if (s->s_itimer.timer_call)
		ret = s->s_itimer.timer_call(s->s_itimer.ident, s->udata);
	else
		ret = DISPATCH_CB_REMOVE;
#if defined(OS_LINUX) && defined(USE_AIO_POLL)
	if (ret == DISPATCH_CB_CONTINUE)
		ret = DISPATCH_CB_READD;
#endif
	return ret;
}

static dispatch_cb_ret_t handle_source_abstimer(dispatch_source_t s, void *event)
{
	int ret = DISPATCH_CB_CONTINUE;
	int overrun = timer_get_overrun(event);
	if (overrun < 0) {
		neb_syslog(LOG_ERR, "Failed to get abstimer overrun");
		return DISPATCH_CB_BREAK;
	}
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
# ifdef USE_AIO_POLL
	struct io_event *e = q->context.ee + i;
	s = (dispatch_source_t)e->data;
# else
	struct epoll_event *e = q->context.ee + i;
	s = e->data.ptr;
# endif
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
	struct kevent *e = q->context.ee + i;
	s = (dispatch_source_t)e->udata;
#elif defined(OS_SOLARIS)
	port_event_t *e = q->context.ee + i;
	s = e->portev_user;
#else
# error "fix me"
#endif

	if (!s) // source has been removed
		return ret;

	switch (s->type) {
	case DISPATCH_SOURCE_FULL_FD:
		ret = handle_source_fd(s, e);
		break;
	case DISPATCH_SOURCE_READ_FD:
		ret = handle_source_read_fd(s, e);
		break;
	case DISPATCH_SOURCE_ITIMER:
		ret = handle_source_itimer(s, e);
		break;
	case DISPATCH_SOURCE_ABSTIMER:
		ret = handle_source_abstimer(s, e);
		break;
	default:
		neb_syslog(LOG_ERR, "Unsupported dispatch source type %d for %p", s->type, s);
		ret = DISPATCH_CB_BREAK;
		break;
	}

	switch (ret) {
	case DISPATCH_CB_REMOVE:
		neb_dispatch_queue_rm(q, s);
		ret = DISPATCH_CB_CONTINUE;
		break;
	case DISPATCH_CB_READD:
		if (s->re_add_immediatly) {
			if (dispatch_queue_readd(q, s) != 0) {
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

int neb_dispatch_queue_run(dispatch_queue_t q)
{
#define BATCH_EVENTS 20
	int ret = 0;
	q->cur_msec = q->get_msec();
	for (;;) {
		if (thread_events) {
			if (q->event_call) {
				if (q->event_call(q->udata) == DISPATCH_CB_BREAK)
					break;
			} else {
				if (thread_events & T_E_QUIT)
					break;

				thread_events = 0;
			}
		}

		int timeout_msec = -1;
		if (q->timer) {
			if (q->update_msec) {
				q->cur_msec = q->get_msec();
				q->update_msec = 0;
			}
			timeout_msec = dispatch_timer_get_min(q->timer, q->cur_msec);
		}
#if defined(OS_LINUX) && !defined(USE_AIO_POLL)
		int timeout = timeout_msec;
#else
		struct timespec ts;
		struct timespec *timeout = NULL;
		if (timeout_msec != -1) {
			ts.tv_sec = timeout_msec / 1000;
			ts.tv_nsec = (timeout_msec % 1000) * 1000000;
			timeout = &ts;
		}
#endif

#if defined(OS_LINUX)
# ifdef USE_AIO_POLL
		q->context.nevents = neb_aio_poll_wait(q->context.id, q->batch_size, q->context.ee, timeout);
		if (q->context.nevents == -1) {
			switch (errno) {
			case EINTR:
				continue;
				break;
			default:
				neb_syslog(LOG_ERR, "(aio %lu)aio_poll_wait: %m", q->context.id);
				ret = -1;
				goto exit_return;
			}
		}
# else
		q->context.nevents = epoll_wait(q->context.fd, q->context.ee, q->batch_size, timeout);
		if (q->context.nevents == -1) {
			switch (errno) {
			case EINTR:
				continue;
				break;
			default:
				neb_syslog(LOG_ERR, "(epoll %d)epoll_wait: %m", q->context.fd);
				ret = -1;
				goto exit_return;
			}
		}
# endif
#elif defined(OSTYPE_BSD) || defined(OS_DARWIN)
		q->context.nevents = kevent(q->context.fd, NULL, 0, q->context.ee, q->batch_size, timeout);
		if (q->context.nevents == -1) {
			switch (errno) {
			case EINTR:
				continue;
				break;
			default:
				neb_syslog(LOG_ERR, "(kqueue %d)kevent: %m", q->context.fd);
				ret = -1;
				goto exit_return;
			}
		}
#elif defined(OS_SOLARIS)
		uint_t nget = 1;
		if (port_getn(q->context.fd, q->context.ee, q->batch_size, &nget, timeout) == -1) {
			switch(errno) {
			case EINTR:
				continue;
				break;
			case ETIME:
				break;
			default:
				neb_syslog(LOG_ERR, "(port %d)port_getn: %m", q->context.fd);
				ret = -1;
				goto exit_return;
			}
		}
		q->context.nevents = nget;
#else
# error "fix me"
#endif
		q->round++;
		q->total_events += q->context.nevents;
		for (int i = 0; i < q->context.nevents; i++) {
			q->context.current_event = i;
			if (handle_event(q, i) == DISPATCH_CB_BREAK)
				goto exit_return;
		}
		if (dispatch_queue_readd_batch(q) != 0) {
			neb_syslog(LOG_ERR, "Failed to batch re-add sources");
			goto exit_return;
		}
		if (q->timer) {
			q->cur_msec = q->get_msec();
			if (dispatch_timer_run_until(q->timer, q->cur_msec) > 0)
				q->update_msec = 1;
		}
		if (q->batch_call) {
			q->update_msec = 1;
			if (q->batch_call(q->udata) == DISPATCH_CB_BREAK)
				goto exit_return;
		}
	}
exit_return:
	return ret;
}
