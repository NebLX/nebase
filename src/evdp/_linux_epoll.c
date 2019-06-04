
#include <nebase/syslog.h>
#include <nebase/time.h>

#include "core.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>

struct evdp_queue_context {
	int fd;
	struct epoll_event *ee;
};

struct evdp_source_conext {
	struct epoll_event ctl_event;
	int ctl_op;
};

struct evdp_source_timer_context {
	struct epoll_event ctl_event;
	int ctl_op;
	int fd;
	struct itimerspec its;
};

void *evdp_create_queue_context(neb_evdp_queue_t q)
{
	struct evdp_queue_context *c = calloc(1, sizeof(struct evdp_queue_context));
	if (!c) {
		neb_syslog(LOG_ERR, "malloc: %m");
		return NULL;
	}
	c->fd = -1;

	c->ee = malloc(q->batch_size * sizeof(struct epoll_event));
	if (!c->ee) {
		neb_syslog(LOG_ERR, "malloc: %m");
		evdp_destroy_queue_context(c);
		return NULL;
	}

	c->fd = epoll_create1(EPOLL_CLOEXEC);
	if (c->fd == -1) {
		neb_syslog(LOG_ERR, "epoll_create1: %m");
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
	struct evdp_queue_context *c = q->context;
	if (!c->ee)
		return;
	for (int i = q->current_event; i < q->nevents; i++) {
		struct epoll_event *e = c->ee + i;
		s_got = e->data.ptr;
		if (s_got == s_to_rm)
			e->data.ptr = NULL;
	}
}

int evdp_queue_wait_events(neb_evdp_queue_t q, struct timespec *timeout)
{
	struct evdp_queue_context *c = q->context;

	int timeout_msec;
	if (timeout) {
		timeout_msec = 0;
		if (timeout->tv_sec)
			timeout_msec += timeout->tv_sec * 1000;
		if (timeout->tv_nsec)
			timeout_msec += timeout->tv_nsec % 1000000;
	} else {
		timeout_msec = -1;
	}

	q->nevents = epoll_wait(c->fd, c->ee, q->batch_size, timeout_msec);
	if (q->nevents == -1) {
		switch (errno) {
		case EINTR:
			q->nevents = 0;
			break;
		default:
			neb_syslog(LOG_ERR, "(epoll %d)epoll_wait: %m", c->fd);
			return -1;
			break;
		}
	}
	return 0;
}

int evdp_queue_fetch_event(neb_evdp_queue_t q, struct neb_evdp_event *nee)
{
	struct evdp_queue_context *c = q->context;

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
		if (epoll_ctl(qc->fd, sc->ctl_op, sc->ctl_event.data.fd, &sc->ctl_event) == -1) {
			neb_syslog(LOG_ERR, "epoll_ctl(op:%d): %m", sc->ctl_op);
			return -1;
		}
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
		neb_syslog(LOG_ERR, "calloc: %m");
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
		neb_syslog(LOG_ERR, "timerfd_create: %m");
		evdp_destroy_source_itimer_context(c);
		return NULL;
	}
	s->pending = 0;
	s->in_action = 0;

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
	struct evdp_source_timer_context *c = s->context;

	if (!s->in_action) {
		if (timerfd_settime(c->fd, 0, &c->its, NULL) == -1) {
			neb_syslog(LOG_ERR, "timerfd_settime: %m");
			return -1;
		}
		s->in_action = 1;
	}

	c->ctl_op = EPOLL_CTL_ADD;
	c->ctl_event.data.fd = c->fd;
	c->ctl_event.data.ptr = s;
	c->ctl_event.events = EPOLLIN;

	EVDP_SLIST_PENDING_INSERT(q, s);

	return 0;
}

void evdp_source_itimer_detach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	struct evdp_queue_context *qc = q->context;
	struct evdp_source_timer_context *sc = s->context;

	if (s->in_action) {
		sc->its.it_value.tv_sec = 0;
		sc->its.it_value.tv_nsec = 0;
		if (timerfd_settime(sc->fd, 0, &sc->its, NULL) == -1)
			neb_syslog(LOG_ERR, "timerfd_settime: %m");
	}
	if (!s->pending) {
		if (epoll_ctl(qc->fd, EPOLL_CTL_DEL, sc->fd, NULL) == -1)
			neb_syslog(LOG_ERR, "epoll_ctl(EPOLL_CTL_DEL): %m");
	}
}

neb_evdp_cb_ret_t evdp_source_itimer_handle(struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	const struct epoll_event *e = ne->event;

	uint64_t overrun = 0;
	if (read(e->data.fd, &overrun, sizeof(overrun)) == -1) {
		neb_syslog(LOG_ERR, "read: %m");
		return NEB_EVDP_CB_BREAK; // should not happen
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
		neb_syslog(LOG_ERR, "calloc: %m");
		return NULL;
	}

	c->its.it_interval.tv_sec = TOTAL_DAY_SECONDS;
	c->its.it_interval.tv_nsec = 0;

	c->fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
	if (c->fd == -1) {
		neb_syslog(LOG_ERR, "timerfd_create: %m");
		evdp_destroy_source_itimer_context(c);
		return NULL;
	}
	s->pending = 0;
	s->in_action = 0;

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
	struct evdp_conf_abstimer *conf = s->conf;

	time_t abs_ts;
	int delta_sec;
	if (neb_daytime_abs_nearest(conf->sec_of_day, &abs_ts, &delta_sec) != 0) {
		neb_syslog(LOG_ERR, "Failed to get next abs time for sec_of_day %d", conf->sec_of_day);
		return -1;
	}

	c->its.it_value.tv_sec = abs_ts;
	c->its.it_value.tv_nsec = 0;

	if (s->in_action) {
		if (timerfd_settime(c->fd, TFD_TIMER_ABSTIME, &c->its, NULL) == -1) {
			neb_syslog(LOG_ERR, "timerfd_settime: %m");
			return -1;
		}
	}

	return 0;
}

int evdp_source_abstimer_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	struct evdp_source_timer_context *c = s->context;

	if (!s->in_action) {
		if (timerfd_settime(c->fd, TFD_TIMER_ABSTIME, &c->its, NULL) == -1) {
			neb_syslog(LOG_ERR, "timerfd_settime: %m");
			return -1;
		}
		s->in_action = 1;
	}

	c->ctl_op = EPOLL_CTL_ADD;
	c->ctl_event.data.fd = c->fd;
	c->ctl_event.data.ptr = s;
	c->ctl_event.events = EPOLLIN;

	EVDP_SLIST_PENDING_INSERT(q, s);

	return 0;
}

void evdp_source_abstimer_detach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	struct evdp_queue_context *qc = q->context;
	struct evdp_source_timer_context *sc = s->context;

	if (s->in_action) {
		sc->its.it_value.tv_sec = 0;
		sc->its.it_value.tv_nsec = 0;
		if (timerfd_settime(sc->fd, TFD_TIMER_ABSTIME, &sc->its, NULL) == -1)
			neb_syslog(LOG_ERR, "timerfd_settime: %m");
	}
	if (!s->pending) {
		if (epoll_ctl(qc->fd, EPOLL_CTL_DEL, sc->fd, NULL) == -1)
			neb_syslog(LOG_ERR, "epoll_ctl(EPOLL_CTL_DEL): %m");
	}
}

neb_evdp_cb_ret_t evdp_source_abstimer_handle(struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	const struct epoll_event *e = ne->event;

	uint64_t overrun = 0;
	if (read(e->data.fd, &overrun, sizeof(overrun)) == -1) {
		neb_syslog(LOG_ERR, "read: %m");
		return NEB_EVDP_CB_BREAK; // should not happen
	}

	struct evdp_conf_itimer *conf = ne->source->conf;
	if (conf->do_wakeup)
		ret = conf->do_wakeup(conf->ident, overrun, ne->source->udata);

	return ret;
}
