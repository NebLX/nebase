
#include <nebase/syslog.h>
#include <nebase/time.h>

#include "core.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/event.h>

struct evdp_queue_context {
	int fd;
	struct kevent *ee;
};

struct evdp_source_timer_context {
	struct kevent ctl_event;
	int attached;
};

void *evdp_create_queue_context(neb_evdp_queue_t q)
{
	struct evdp_queue_context *c = calloc(1, sizeof(struct evdp_queue_context));
	if (!c) {
		neb_syslog(LOG_ERR, "malloc: %m");
		return NULL;
	}
	c->fd = -1;

	c->ee = malloc(q->batch_size * sizeof(struct kevent));
	if (!c->ee) {
		neb_syslog(LOG_ERR, "malloc: %m");
		evdp_destroy_queue_context(c);
		return NULL;
	}

	c->fd = kqueue();
	if (c->fd == -1) {
		neb_syslog(LOG_ERR, "kqueue: %m");
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
		struct kevent *e = c->ee + i;
		s_got = (neb_evdp_source_t)e->udata;
		if (s_got == s_to_rm)
# if defined(OS_NETBSD)
			e->udata = (intptr_t)NULL;
# else
			e->udata = NULL;
# endif
	}
}

int evdp_queue_wait_events(neb_evdp_queue_t q, struct timespec *timeout)
{
	struct evdp_queue_context *c = q->context;

	int readd_nevents = q->nevents; // re add events first
	q->nevents = kevent(c->fd, c->ee, readd_nevents, c->ee, q->batch_size, timeout);
	if (q->nevents == -1) {
		switch (errno) {
		case EINTR:
			q->nevents = 0;
			break;
		default:
			neb_syslog(LOG_ERR, "(kqueue %d)kevent: %m", c->fd);
			return -1;
			break;
		}
	}
	return 0;
}

int evdp_queue_fetch_event(neb_evdp_queue_t q, struct neb_evdp_event *nee)
{
	struct evdp_queue_context *c = q->context;

	struct kevent *e = c->ee + q->current_event;
	nee->event = e;
	nee->source = (neb_evdp_source_t)e->udata;
	if (e->flags & EV_ERROR) { // see return value of kevent
		neb_syslog_en(e->data, LOG_ERR, "kevent: %m");
		return -1;
	}
	return 0;
}

static int do_batch_flush(neb_evdp_queue_t q, int nr)
{
	struct evdp_queue_context *qc = q->context;
	if (kevent(qc->fd, qc->ee, nr, NULL, 0, NULL) == -1) {
		neb_syslog(LOG_ERR, "kevent: %m");
		return -1;
	}
	q->stats.pending -= nr;
	q->stats.running += nr;
	for (int i = 0; i < nr; i++) {
		neb_evdp_source_t s = (neb_evdp_source_t)qc->ee[i].udata;
		EVDP_SLIST_REMOVE(s);
		EVDP_SLIST_RUNNING_INSERT_NO_STATS(q, s);
	}
	return 0;
}

int evdp_queue_flush_pending_sources(neb_evdp_queue_t q)
{
	struct evdp_queue_context *qc = q->context;
	int count = 0;
	neb_evdp_source_t last_s = NULL;
	for (neb_evdp_source_t s = q->pending_qs->next; s && last_s != s; s = q->pending_qs->next) {
		last_s = s;
		switch (s->type) {
		case EVDP_SOURCE_ITIMER_SEC:
		case EVDP_SOURCE_ITIMER_MSEC:
		case EVDP_SOURCE_ABSTIMER:
		{
			struct evdp_source_timer_context *sc = s->context;
			memcpy(qc->ee + count++, &sc->ctl_event, sizeof(sc->ctl_event));
		}
			break;
		case EVDP_SOURCE_RO_FD: // TODO
		case EVDP_SOURCE_OS_FD: // TODO
		case EVDP_SOURCE_LT_FD:
		default:
			neb_syslog(LOG_ERR, "Unsupported pending source type %d", s->type);
			return -1;
			break;
		}
		if (count >= q->batch_size) {
			if (do_batch_flush(q, count) != 0)
				return -1;
			count = 0;
		}
	}
	if (count) // the last ones will be added during wait_events
		q->nevents = count;
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
	unsigned int fflags = 0;
	int64_t data = 0;
	switch (s->type) {
	case EVDP_SOURCE_ITIMER_SEC:
#ifdef NOTE_SECONDS
		fflags |= NOTE_SECONDS;
		data = conf->sec;
#else
		data = conf->sec * 1000;
#endif
		break;
	case EVDP_SOURCE_ITIMER_MSEC:
		data = conf->msec;
		break;
	default:
		neb_syslog(LOG_CRIT, "Invalid itimer source type");
		evdp_destroy_source_itimer_context(c);
		return NULL;
		break;
	}

	EV_SET(&c->ctl_event, conf->ident, EVFILT_TIMER, EV_ADD | EV_ENABLE, fflags, data, s);
	c->attached = 0;
	s->pending = 0;
	s->in_action = 0;

	return c;
}

void evdp_destroy_source_itimer_context(void *context)
{
	struct evdp_source_timer_context *c = context;

	free(c);
}

int evdp_source_itimer_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	struct evdp_source_timer_context *c = s->context;

	EVDP_SLIST_PENDING_INSERT(q, s);
	c->attached = 1;

	return 0;
}

void evdp_source_itimer_detach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	struct evdp_source_timer_context *c = s->context;
	if (c->attached && !s->pending) {
		struct evdp_queue_context *qc = q->context;
		struct evdp_conf_itimer *conf = s->conf;
		struct kevent e;
		EV_SET(&e, conf->ident, EVFILT_TIMER, EV_DISABLE | EV_DELETE, 0, 0, NULL);
		if (kevent(qc->fd, &e, 1, NULL, 0, NULL) == -1)
			neb_syslog(LOG_ERR, "kevent: %m");
	}
	c->attached = 0;
}

neb_evdp_cb_ret_t evdp_source_itimer_handle(struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	const struct kevent *e = ne->event;
	int64_t overrun = e->data;

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

	c->attached = 0;
	s->pending = 0;
	s->in_action = 0;

	return c;
}

void evdp_destroy_source_abstimer_context(void *context)
{
	struct evdp_source_timer_context *c = context;

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

	unsigned int fflags = 0;
	int64_t data = 0;

#ifdef NOTE_ABSTIME
	fflags |= NOTE_ABSTIME;
	data = abs_ts;
#else
	data = delta_sec;
#endif

#ifdef NOTE_SECONDS
	fflags |= NOTE_SECONDS;
#else
	data *= 1000;
#endif

	EV_SET(&c->ctl_event, conf->ident, EVFILT_TIMER, EV_ADD | EV_ENABLE | EV_ONESHOT, fflags, data, s);

	if (c->attached && !s->pending) {
		neb_evdp_queue_t q = s->q_in_use;
		struct evdp_queue_context *qc = q->context;
		if (kevent(qc->fd, &c->ctl_event, 1, NULL, 0, NULL) == -1) {
			neb_syslog(LOG_ERR, "kevent: %m");
			return -1;
		}
	}

	return 0;
}

int evdp_source_abstimer_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	struct evdp_source_timer_context *c = s->context;

	EVDP_SLIST_PENDING_INSERT(q, s);
	c->attached = 1;

	return 0;
}

void evdp_source_abstimer_detach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	struct evdp_source_timer_context *c = s->context;
	if (c->attached && !s->pending) {
		struct evdp_queue_context *qc = q->context;
		struct evdp_conf_abstimer *conf = s->conf;
		struct kevent e;
		EV_SET(&e, conf->ident, EVFILT_TIMER, EV_DISABLE | EV_DELETE, 0, 0, NULL);
		if (kevent(qc->fd, &e, 1, NULL, 0, NULL) == -1)
			neb_syslog(LOG_ERR, "kevent: %m");
	}
	c->attached = 0;
}

neb_evdp_cb_ret_t evdp_source_abstimer_handle(struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	struct evdp_source_timer_context *c = ne->source->context;
	c->attached = 0;

	const struct kevent *e = ne->event;
	int64_t overrun = e->data;

	struct evdp_conf_abstimer *conf = ne->source->conf;
	if (conf->do_wakeup)
		ret = conf->do_wakeup(conf->ident, overrun, ne->source->udata);
	if (ret == NEB_EVDP_CB_CONTINUE && conf->interval_hour) {
		if (conf->interval_hour % 24 != 0) {
			conf->sec_of_day += conf->interval_hour * 3600;
			if (conf->sec_of_day >= 24 * 3600)
				conf->sec_of_day = conf->sec_of_day % 24 * 3600;
		}
		if (evdp_source_abstimer_regulate(ne->source) != 0) {
			neb_syslog(LOG_ERR, "Failed to regulate abstimer source");
			return NEB_EVDP_CB_BREAK;
		}
		neb_evdp_queue_t q = ne->source->q_in_use;
		EVDP_SLIST_REMOVE(ne->source);
		q->stats.running--;
		EVDP_SLIST_PENDING_INSERT(q, ne->source);
		c->attached = 1;
	}

	return ret;
}
