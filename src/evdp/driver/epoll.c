
#include <nebase/syslog.h>
#include <nebase/time.h>

#include "core.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

struct evdp_queue_context {
	int fd;
	struct epoll_event *ee;
};

struct evdp_source_conext {
	struct epoll_event ctl_event;
	int ctl_op;
	int added;
};

struct evdp_source_timer_context {
	struct epoll_event ctl_event;
	int ctl_op;
	int added;
	int in_action;
	int fd;
	struct itimerspec its;
};

struct evdp_source_ro_fd_context {
	struct epoll_event ctl_event;
	int ctl_op;
	int added;
};

struct evdp_source_os_fd_context {
	struct epoll_event ctl_event;
	int ctl_op;
	int added;
};

void *evdp_create_queue_context(neb_evdp_queue_t q)
{
	struct evdp_queue_context *c = calloc(1, sizeof(struct evdp_queue_context));
	if (!c) {
		neb_syslogl(LOG_ERR, "malloc: %m");
		return NULL;
	}
	c->fd = -1;

	c->ee = malloc(q->batch_size * sizeof(struct epoll_event));
	if (!c->ee) {
		neb_syslogl(LOG_ERR, "malloc: %m");
		evdp_destroy_queue_context(c);
		return NULL;
	}

	c->fd = epoll_create1(EPOLL_CLOEXEC);
	if (c->fd == -1) {
		neb_syslogl(LOG_ERR, "epoll_create1: %m");
		evdp_destroy_queue_context(c);
		return NULL;
	}

	return c;
}

void evdp_destroy_queue_context(void *context)
{
	struct evdp_queue_context *c = context;

	if (c->fd >= 0)
		close(c->fd);
	if (c->ee)
		free(c->ee);
	free(c);
}

void evdp_queue_rm_pending_events(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	void *s_got = NULL, *s_to_rm = s;
	const struct evdp_queue_context *c = q->context;
	if (!c->ee)
		return;
	for (int i = q->current_event; i < q->nevents; i++) {
		struct epoll_event *e = c->ee + i;
		s_got = e->data.ptr;
		if (s_got == s_to_rm)
			e->data.ptr = NULL;
	}
}

int evdp_queue_wait_events(neb_evdp_queue_t q, int timeout_msec)
{
	const struct evdp_queue_context *c = q->context;

	q->nevents = epoll_wait(c->fd, c->ee, q->batch_size, timeout_msec);
	if (q->nevents == -1) {
		switch (errno) {
		case EINTR:
			q->nevents = 0;
			break;
		default:
			neb_syslogl(LOG_ERR, "epoll_wait: %m");
			return -1;
			break;
		}
	}
	return 0;
}

int evdp_queue_fetch_event(neb_evdp_queue_t q, struct neb_evdp_event *nee)
{
	const struct evdp_queue_context *c = q->context;

	struct epoll_event *e = c->ee + q->current_event;
	nee->event = e;
	nee->source = e->data.ptr;
	return 0;
}

int evdp_queue_flush_pending_sources(neb_evdp_queue_t q)
{
	struct evdp_queue_context *qc = q->context;
	int count = 0;
	for (neb_evdp_source_t s = q->pending_qs->next; s; s = q->pending_qs->next) {
		struct evdp_source_conext *sc = s->context;
		int fd;
		switch (s->type) {
		case EVDP_SOURCE_ITIMER_SEC:
		case EVDP_SOURCE_ITIMER_MSEC:
		case EVDP_SOURCE_ABSTIMER:
			fd = ((struct evdp_source_timer_context *)s->context)->fd;
			break;
		case EVDP_SOURCE_RO_FD:
			fd = ((struct evdp_conf_ro_fd *)s->conf)->fd;
			break;
		case EVDP_SOURCE_OS_FD:
			fd = ((struct evdp_conf_fd *)s->conf)->fd;
			break;
		case EVDP_SOURCE_LT_FD: // TODO
		default:
			neb_syslog(LOG_ERR, "Unsupported epoll(ADD/MOD) source type %d", s->type);
			return -1;
			break;
		}
		if (epoll_ctl(qc->fd, sc->ctl_op, fd, &sc->ctl_event) == -1) {
			neb_syslogl(LOG_ERR, "epoll_ctl(op:%d): %m", sc->ctl_op);
			return -1;
		}
		sc->added = 1;
		EVDP_SLIST_REMOVE(s);
		EVDP_SLIST_RUNNING_INSERT_NO_STATS(q, s);
		count++;
	}
	if (count) {
		q->stats.pending -= count;
		q->stats.running += count;
	}
	return 0;
}

void *evdp_create_source_itimer_context(neb_evdp_source_t s)
{
	struct evdp_source_timer_context *c = calloc(1, sizeof(struct evdp_source_timer_context));
	if (!c) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	struct evdp_conf_itimer *conf = s->conf;
	switch (s->type) {
	case EVDP_SOURCE_ITIMER_SEC:
		c->its.it_value.tv_sec = conf->sec;
		c->its.it_interval.tv_sec = c->its.it_value.tv_sec;
		break;
	case EVDP_SOURCE_ITIMER_MSEC:
		c->its.it_value.tv_nsec = conf->msec * 1000000;
		c->its.it_interval.tv_nsec = c->its.it_value.tv_nsec;
		break;
	default:
		neb_syslog(LOG_CRIT, "Invalid itimer source type");
		evdp_destroy_source_itimer_context(c);
		return NULL;
		break;
	}

	c->fd = timerfd_create(CLOCK_BOOTTIME, TFD_NONBLOCK | TFD_CLOEXEC);
	if (c->fd == -1) {
		neb_syslogl(LOG_ERR, "timerfd_create: %m");
		evdp_destroy_source_itimer_context(c);
		return NULL;
	}

	c->in_action = 0;
	c->added = 0;
	s->pending = 0;

	return c;
}

void evdp_destroy_source_itimer_context(void *context)
{
	struct evdp_source_timer_context *c = context;

	if (c->fd >= 0)
		close(c->fd);
	free(c);
}

int evdp_source_itimer_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	struct evdp_source_timer_context *sc = s->context;

	if (!sc->in_action) {
		if (timerfd_settime(sc->fd, 0, &sc->its, NULL) == -1) {
			neb_syslogl(LOG_ERR, "timerfd_settime: %m");
			return -1;
		}
		sc->in_action = 1;
	}

	sc->ctl_op = EPOLL_CTL_ADD;
	sc->ctl_event.data.ptr = s;
	sc->ctl_event.events = EPOLLIN;

	EVDP_SLIST_PENDING_INSERT(q, s);

	return 0;
}

void evdp_source_itimer_detach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	const struct evdp_queue_context *qc = q->context;
	struct evdp_source_timer_context *sc = s->context;

	if (sc->in_action) {
		sc->its.it_value.tv_sec = 0;
		sc->its.it_value.tv_nsec = 0;
		if (timerfd_settime(sc->fd, 0, &sc->its, NULL) == -1)
			neb_syslogl(LOG_ERR, "timerfd_settime: %m");
		sc->in_action = 0;
	}
	if (sc->added) {
		if (epoll_ctl(qc->fd, EPOLL_CTL_DEL, sc->fd, NULL) == -1)
			neb_syslogl(LOG_ERR, "epoll_ctl(EPOLL_CTL_DEL): %m");
		sc->added = 0;
	}
}

neb_evdp_cb_ret_t evdp_source_itimer_handle(const struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	const struct evdp_source_timer_context *c = ne->source->context;

	uint64_t overrun = 0;
	if (read(c->fd, &overrun, sizeof(overrun)) == -1) {
		neb_syslogl(LOG_ERR, "read: %m");
		return NEB_EVDP_CB_BREAK_ERR; // should not happen
	}

	struct evdp_conf_itimer *conf = ne->source->conf;
	if (conf->do_wakeup)
		ret = conf->do_wakeup(conf->ident, overrun, ne->source->udata);

	return ret;
}

void *evdp_create_source_abstimer_context(neb_evdp_source_t s)
{
	struct evdp_source_timer_context *c = calloc(1, sizeof(struct evdp_source_timer_context));
	if (!c) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	c->its.it_interval.tv_sec = TOTAL_DAY_SECONDS;
	c->its.it_interval.tv_nsec = 0;

	c->fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
	if (c->fd == -1) {
		neb_syslogl(LOG_ERR, "timerfd_create: %m");
		evdp_destroy_source_itimer_context(c);
		return NULL;
	}

	c->in_action = 0;
	c->added = 0;
	s->pending = 0;

	return c;
}

void evdp_destroy_source_abstimer_context(void *context)
{
	struct evdp_source_timer_context *c = context;

	if (c->fd >= 0)
		close(c->fd);
	free(c);
}

int evdp_source_abstimer_regulate(neb_evdp_source_t s)
{
	struct evdp_source_timer_context *c = s->context;
	const struct evdp_conf_abstimer *conf = s->conf;

	time_t abs_ts;
	int delta_sec;
	if (neb_daytime_abs_nearest(conf->sec_of_day, &abs_ts, &delta_sec) != 0) {
		neb_syslog(LOG_ERR, "Failed to get next abs time for sec_of_day %d", conf->sec_of_day);
		return -1;
	}

	c->its.it_value.tv_sec = abs_ts;
	c->its.it_value.tv_nsec = 0;

	if (c->in_action) {
		if (timerfd_settime(c->fd, TFD_TIMER_ABSTIME, &c->its, NULL) == -1) {
			neb_syslogl(LOG_ERR, "timerfd_settime: %m");
			return -1;
		}
	}

	return 0;
}

int evdp_source_abstimer_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	struct evdp_source_timer_context *sc = s->context;

	if (!sc->in_action) {
		if (timerfd_settime(sc->fd, TFD_TIMER_ABSTIME, &sc->its, NULL) == -1) {
			neb_syslogl(LOG_ERR, "timerfd_settime: %m");
			return -1;
		}
		sc->in_action = 1;
	}

	sc->ctl_op = EPOLL_CTL_ADD;
	sc->ctl_event.data.ptr = s;
	sc->ctl_event.events = EPOLLIN;

	EVDP_SLIST_PENDING_INSERT(q, s);

	return 0;
}

void evdp_source_abstimer_detach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	const struct evdp_queue_context *qc = q->context;
	struct evdp_source_timer_context *sc = s->context;

	if (sc->in_action) {
		sc->its.it_value.tv_sec = 0;
		sc->its.it_value.tv_nsec = 0;
		if (timerfd_settime(sc->fd, TFD_TIMER_ABSTIME, &sc->its, NULL) == -1)
			neb_syslogl(LOG_ERR, "timerfd_settime: %m");
		sc->in_action = 0;
	}
	if (sc->added) {
		if (epoll_ctl(qc->fd, EPOLL_CTL_DEL, sc->fd, NULL) == -1)
			neb_syslogl(LOG_ERR, "epoll_ctl(EPOLL_CTL_DEL): %m");
		sc->added = 0;
	}
}

neb_evdp_cb_ret_t evdp_source_abstimer_handle(const struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	const struct evdp_source_timer_context *c = ne->source->context;

	uint64_t overrun = 0;
	if (read(c->fd, &overrun, sizeof(overrun)) == -1) {
		neb_syslogl(LOG_ERR, "read: %m");
		return NEB_EVDP_CB_BREAK_ERR; // should not happen
	}

	const struct evdp_conf_itimer *conf = ne->source->conf;
	if (conf->do_wakeup)
		ret = conf->do_wakeup(conf->ident, overrun, ne->source->udata);

	return ret;
}

int neb_evdp_source_fd_get_sockerr(const void *context, int *sockerr)
{
	const int *fdp = context;

	socklen_t len = sizeof(int);
	if (getsockopt(*fdp, SOL_SOCKET, SO_ERROR, sockerr, &len) == -1) {
		neb_syslogl(LOG_ERR, "getsockopt(SO_ERROR): %m");
		return -1;
	}

	return 0;
}

int neb_evdp_source_fd_get_nread(const void *context, int *nbytes)
{
	const int *fdp = context;

	if (ioctl(*fdp, FIONREAD, nbytes) == -1) {
		neb_syslogl(LOG_ERR, "ioctl(FIONREAD): %m");
		return -1;
	}

	return 0;
}

void *evdp_create_source_ro_fd_context(neb_evdp_source_t s)
{
	struct evdp_source_ro_fd_context *c = calloc(1, sizeof(struct evdp_source_ro_fd_context));
	if (!c) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	c->added = 0;
	s->pending = 0;

	return c;
}

void evdp_destroy_source_ro_fd_context(void *context)
{
	struct evdp_source_ro_fd_context *c = context;

	free(c);
}

int evdp_source_ro_fd_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	struct evdp_source_ro_fd_context *sc = s->context;

	sc->ctl_op = EPOLL_CTL_ADD;
	sc->ctl_event.data.ptr = s;
	sc->ctl_event.events = EPOLLIN;

	EVDP_SLIST_PENDING_INSERT(q, s);

	return 0;
}

void evdp_source_ro_fd_detach(neb_evdp_queue_t q, neb_evdp_source_t s, int to_close)
{
	const struct evdp_queue_context *qc = q->context;
	struct evdp_source_ro_fd_context *sc = s->context;
	const struct evdp_conf_ro_fd *conf = s->conf;

	if (to_close) {
		sc->added = 0;
		return;
	}

	if (sc->added) {
		if (epoll_ctl(qc->fd, EPOLL_CTL_DEL, conf->fd, NULL) == -1)
			neb_syslogl(LOG_ERR, "epoll_ctl: %m");
		sc->added = 0;
	}
}

neb_evdp_cb_ret_t evdp_source_ro_fd_handle(const struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	const struct epoll_event *e = ne->event;

	const struct evdp_conf_ro_fd *conf = ne->source->conf;
	if (e->events & EPOLLIN) {
		ret = conf->do_read(conf->fd, ne->source->udata, &conf->fd);
		if (ret != NEB_EVDP_CB_CONTINUE)
			return ret;
	}
	if (e->events & EPOLLHUP) {
		ret = conf->do_hup(conf->fd, ne->source->udata, &conf->fd);
		switch (ret) {
		case NEB_EVDP_CB_BREAK_ERR:
		case NEB_EVDP_CB_BREAK_EXP:
		case NEB_EVDP_CB_CLOSE:
			return ret;
			break;
		default:
			return NEB_EVDP_CB_REMOVE;
			break;
		}
	}

	return ret;
}

void *evdp_create_source_os_fd_context(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *c = calloc(1, sizeof(struct evdp_source_os_fd_context));
	if (!c) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	c->added = 0;
	s->pending = 0;

	return c;
}

void evdp_reset_source_os_fd_context(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *c = s->context;

	c->added = 0;
	s->pending = 0;
	c->ctl_event.events = 0;
}

void evdp_destroy_source_os_fd_context(void *context)
{
	struct evdp_source_os_fd_context *c = context;

	free(c);
}

int evdp_source_os_fd_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;

	sc->ctl_op = EPOLL_CTL_ADD;
	sc->ctl_event.data.ptr = s;
	sc->ctl_event.events |= EPOLLONESHOT; // the real event is dynamic

	if (sc->ctl_event.events & (EPOLLIN | EPOLLOUT)) {
		EVDP_SLIST_PENDING_INSERT(q, s);
	} else {
		EVDP_SLIST_RUNNING_INSERT(q, s);
	}

	return 0;
}

static int do_ctl_os_fd(const struct evdp_queue_context *qc, neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	const struct evdp_conf_fd *conf = s->conf;
	if (epoll_ctl(qc->fd, sc->ctl_op, conf->fd, &sc->ctl_event) == -1) {
		neb_syslogl(LOG_ERR, "epoll_ctl(op:%d): %m", sc->ctl_op);
		return -1;
	}
	sc->added = 1;
	return 0;
}

static int do_del_os_fd(const struct evdp_queue_context *qc, neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	const struct evdp_conf_fd *conf = s->conf;
	if (epoll_ctl(qc->fd, EPOLL_CTL_DEL, conf->fd, NULL) == -1) {
		if (errno == ENOENT)
			sc->added = 0;
		neb_syslogl(LOG_ERR, "epoll_ctl: %m");
		return -1;
	}
	sc->added = 0;
	sc->ctl_op = EPOLL_CTL_ADD;
	return 0;
}

void evdp_source_os_fd_detach(neb_evdp_queue_t q, neb_evdp_source_t s, int to_close)
{
	const struct evdp_queue_context *qc = q->context;
	struct evdp_source_os_fd_context *sc = s->context;

	if (to_close) {
		sc->added = 0;
		return;
	}

	if (sc->added)
		do_del_os_fd(qc, s);
}

neb_evdp_cb_ret_t evdp_source_os_fd_handle(const struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	neb_evdp_source_t s = ne->source;
	struct evdp_source_os_fd_context *sc = s->context;
	sc->added = 0;
	sc->ctl_op = EPOLL_CTL_MOD;

	const struct epoll_event *e = ne->event;

	const struct evdp_conf_fd *conf = s->conf;
	if ((e->events & EPOLLIN) && conf->do_read) {
		sc->ctl_event.events &= ~EPOLLIN;
		ret = conf->do_read(conf->fd, s->udata, &conf->fd);
		if (ret != NEB_EVDP_CB_CONTINUE)
			return ret;
	}
	if (e->events & EPOLLHUP) {
		ret = conf->do_hup(conf->fd, s->udata, &conf->fd);
		switch (ret) {
		case NEB_EVDP_CB_BREAK_ERR:
		case NEB_EVDP_CB_BREAK_EXP:
		case NEB_EVDP_CB_CLOSE:
			return ret;
			break;
		default:
			return NEB_EVDP_CB_REMOVE;
			break;
		}
	}
	if ((e->events & EPOLLOUT) && conf->do_write) {
		sc->ctl_event.events &= ~EPOLLOUT;
		ret = conf->do_write(conf->fd, s->udata, &conf->fd);
		if (ret != NEB_EVDP_CB_CONTINUE)
			return ret;
	}

	if (sc->ctl_event.events & (EPOLLIN | EPOLLOUT)) { // do pending if only handled one of them
		neb_evdp_queue_t q = s->q_in_use;
		EVDP_SLIST_REMOVE(s);
		q->stats.running--;
		EVDP_SLIST_PENDING_INSERT(q, s);
	}

	return ret;
}

void evdp_source_os_fd_init_read(neb_evdp_source_t s, neb_evdp_io_handler_t rf)
{
	struct evdp_source_os_fd_context *sc = s->context;
	if (rf)
		sc->ctl_event.events |= EPOLLIN;
	else
		sc->ctl_event.events &= ~EPOLLIN;
}

void evdp_source_os_fd_init_write(neb_evdp_source_t s, neb_evdp_io_handler_t rf)
{
	struct evdp_source_os_fd_context *sc = s->context;
	if (rf)
		sc->ctl_event.events |= EPOLLOUT;
	else
		sc->ctl_event.events &= ~EPOLLOUT;
}

int evdp_source_os_fd_reset_read(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	if (sc->added) {
		if (sc->ctl_event.events & EPOLLIN)
			return 0;
		sc->ctl_event.events |= EPOLLIN;
		return do_ctl_os_fd(s->q_in_use->context, s);
	} else {
		sc->ctl_event.events |= EPOLLIN;
		if (!s->pending) { // Make sure add to pending
			neb_evdp_queue_t q = s->q_in_use;
			EVDP_SLIST_REMOVE(s);
			q->stats.running--;
			EVDP_SLIST_PENDING_INSERT(q, s);
		}
	}
	return 0;
}

int evdp_source_os_fd_reset_write(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	if (sc->added) {
		if (sc->ctl_event.events & EPOLLOUT)
			return 0;
		sc->ctl_event.events |= EPOLLOUT;
		return do_ctl_os_fd(s->q_in_use->context, s);
	} else {
		sc->ctl_event.events |= EPOLLOUT;
		if (!s->pending) { // Make sure add to pending
			neb_evdp_queue_t q = s->q_in_use;
			EVDP_SLIST_REMOVE(s);
			q->stats.running--;
			EVDP_SLIST_PENDING_INSERT(q, s);
		}
	}
	return 0;
}

int evdp_source_os_fd_unset_read(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	if (sc->added) {
		if (!(sc->ctl_event.events & EPOLLIN))
			return 0;
		sc->ctl_event.events ^= EPOLLIN;
		if (sc->ctl_event.events & EPOLLOUT)
			return 0;
		return do_del_os_fd(s->q_in_use->context, s);
	} else if (s->pending) {
		sc->ctl_event.events &= ~EPOLLIN;
		if (sc->ctl_event.events & EPOLLOUT)
			return 0;
		neb_evdp_queue_t q = s->q_in_use;
		EVDP_SLIST_REMOVE(s);
		q->stats.pending--;
		EVDP_SLIST_RUNNING_INSERT(q, s);
	} else {
		sc->ctl_event.events &= ~EPOLLIN;
	}
	return 0;
}

int evdp_source_os_fd_unset_write(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	if (sc->added) {
		if (!(sc->ctl_event.events & EPOLLOUT))
			return 0;
		sc->ctl_event.events ^= EPOLLOUT;
		if (sc->ctl_event.events & EPOLLIN)
			return 0;
		return do_del_os_fd(s->q_in_use->context, s);
	} else if (s->pending) {
		sc->ctl_event.events &= ~EPOLLOUT;
		if (sc->ctl_event.events & EPOLLIN)
			return 0;
		neb_evdp_queue_t q = s->q_in_use;
		EVDP_SLIST_REMOVE(s);
		q->stats.pending--;
		EVDP_SLIST_RUNNING_INSERT(q, s);
	} else {
		sc->ctl_event.events &= ~EPOLLOUT;
	}
	return 0;
}