
#include <nebase/syslog.h>
#include <nebase/time.h>

#include "core.h"
#include "sys_timer.h"
#include "types.h"

#include <stdlib.h>
#include <signal.h>
#include <time.h>

void *evdp_create_source_abstimer_context(neb_evdp_source_t s)
{
	struct evdp_source_timer_context *c = calloc(1, sizeof(struct evdp_source_timer_context));
	if (!c) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	c->its.it_interval.tv_sec = TOTAL_DAY_SECONDS;
	c->its.it_interval.tv_nsec = 0;

	c->created = 0;
	c->in_action = 0;
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

	c->its.it_value.tv_sec = abs_ts;
	c->its.it_value.tv_nsec = 0;

	if (c->in_action) {
		if (timer_settime(c->id, TIMER_ABSTIME, &c->its, NULL) == -1) {
			neb_syslogl(LOG_ERR, "timer_settime: %m");
			return -1;
		}
	}

	return 0;
}

int evdp_source_abstimer_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
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
	if (timer_create(CLOCK_REALTIME, &e, &sc->id) == -1) {
		neb_syslogl(LOG_ERR, "timer_create: %m");
		return -1;
	}
	sc->created = 1;

	if (timer_settime(sc->id, TIMER_ABSTIME, &sc->its, NULL) == -1) {
		neb_syslogl(LOG_ERR, "timer_settime: %m");
		timer_delete(sc->id);
		return -1;
	}
	sc->in_action = 1;

	EVDP_SLIST_RUNNING_INSERT(q, s);

	return 0;
}

void evdp_source_abstimer_detach(neb_evdp_queue_t q _nattr_unused, neb_evdp_source_t s)
{
	struct evdp_source_timer_context *sc = s->context;

	sc->in_action = 0;

	if (sc->created) {
		if (timer_delete(sc->id) == -1)
			neb_syslogl(LOG_ERR, "timer_delete: %m");
		sc->created = 0;
	}
}

neb_evdp_cb_ret_t evdp_source_abstimer_handle(const struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	const port_event_t *e = ne->event;
	int overrun = e->portev_events;

	const struct evdp_conf_itimer *conf = ne->source->conf;
	if (conf->do_wakeup)
		ret = conf->do_wakeup(conf->ident, overrun, ne->source->udata);

	return ret;
}
