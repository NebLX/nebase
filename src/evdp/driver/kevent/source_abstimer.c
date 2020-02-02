
#include <nebase/syslog.h>
#include <nebase/time.h>

#include "core.h"
#include "sys_timer.h"
#include "types.h"

#include <stdlib.h>
#include <errno.h>

void *evdp_create_source_abstimer_context(neb_evdp_source_t s)
{
	struct evdp_source_timer_context *c = calloc(1, sizeof(struct evdp_source_timer_context));
	if (!c) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	c->attached = 0;
	s->pending = 0;

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
	const struct evdp_conf_abstimer *conf = s->conf;

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
			neb_syslogl(LOG_ERR, "kevent: %m");
			return -1;
		}
	}

	return 0;
}

int evdp_source_abstimer_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	struct evdp_source_timer_context *sc = s->context;

	EVDP_SLIST_PENDING_INSERT(q, s);
	sc->attached = 1;

	return 0;
}

void evdp_source_abstimer_detach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	struct evdp_source_timer_context *c = s->context;
	if (c->attached && !s->pending) {
		const struct evdp_queue_context *qc = q->context;
		const struct evdp_conf_abstimer *conf = s->conf;
		struct kevent e;
		EV_SET(&e, conf->ident, EVFILT_TIMER, EV_DISABLE | EV_DELETE, 0, 0, NULL);
		if (kevent(qc->fd, &e, 1, NULL, 0, NULL) == -1 && errno != ENOENT)
			neb_syslogl(LOG_ERR, "kevent: %m");
	}
	c->attached = 0;
}

neb_evdp_cb_ret_t evdp_source_abstimer_handle(const struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	struct evdp_source_timer_context *c = ne->source->context;
	c->attached = 0;

	const struct kevent *e = ne->event;
	int64_t overrun = e->data;

	const struct evdp_conf_abstimer *conf = ne->source->conf;
	if (conf->do_wakeup)
		ret = conf->do_wakeup(conf->ident, overrun, ne->source->udata);
	if (ret == NEB_EVDP_CB_CONTINUE) {
		// Try to use abstime instead of relative time
		if (evdp_source_abstimer_regulate(ne->source) != 0) {
			neb_syslog(LOG_ERR, "Failed to regulate abstimer source");
			return NEB_EVDP_CB_BREAK_ERR;
		}
		neb_evdp_queue_t q = ne->source->q_in_use;
		EVDP_SLIST_REMOVE(ne->source);
		q->stats.running--;
		EVDP_SLIST_PENDING_INSERT(q, ne->source);
		c->attached = 1;
	}

	return ret;
}
