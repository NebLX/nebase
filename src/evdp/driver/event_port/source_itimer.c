
#include <nebase/syslog.h>

#include "core.h"
#include "types.h"

#include <stdlib.h>
#include <signal.h>
#include <time.h>

void *evdp_create_source_itimer_context(neb_evdp_source_t s)
{
	struct evdp_source_timer_context *c = calloc(1, sizeof(struct evdp_source_timer_context));
	if (!c) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	const struct evdp_conf_itimer *conf = s->conf;
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

	c->created = 0;
	c->in_action = 0;
	s->pending = 0;

	return c;
}

void evdp_destroy_source_itimer_context(void *context)
{
	struct evdp_source_timer_context *c = context;

	free(c);
}

int evdp_source_itimer_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	const struct evdp_queue_context *qc = q->context;
	struct evdp_source_timer_context *sc = s->context;

	port_notify_t pn = {
		.portnfy_port = qc->fd,
		.portnfy_user = s,
	};
	struct sigevent e = {
		.sigev_notify = SIGEV_PORT,
		.sigev_value.sival_ptr = &pn,
	};
	if (timer_create(CLOCK_MONOTONIC, &e, &sc->id) == -1) {
		neb_syslogl(LOG_ERR, "timer_create: %m");
		return -1;
	}
	sc->created = 1;

	if (timer_settime(sc->id, 0, &sc->its, NULL) == -1) {
		neb_syslogl(LOG_ERR, "timer_settime: %m");
		timer_delete(sc->id);
		return -1;
	}
	sc->in_action = 1;

	EVDP_SLIST_RUNNING_INSERT(q, s);

	return 0;
}

void evdp_source_itimer_detach(neb_evdp_queue_t q _nattr_unused, neb_evdp_source_t s)
{
	struct evdp_source_timer_context *sc = s->context;

	sc->in_action = 0;

	if (sc->created) {
		if (timer_delete(sc->id) == -1)
			neb_syslogl(LOG_ERR, "timer_delete: %m");
		sc->created = 0;
	}
}

neb_evdp_cb_ret_t evdp_source_itimer_handle(const struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	const port_event_t *e = ne->event;
	int overrun = e->portev_events;

	const struct evdp_conf_itimer *conf = ne->source->conf;
	if (conf->do_wakeup)
		ret = conf->do_wakeup(conf->ident, overrun, ne->source->udata);

	return ret;
}
