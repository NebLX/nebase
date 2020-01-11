
#include <nebase/syslog.h>

#include "core.h"
#include "types.h"

#include <stdlib.h>
#include <errno.h>

void *evdp_create_source_itimer_context(neb_evdp_source_t s)
{
	struct evdp_source_timer_context *c = calloc(1, sizeof(struct evdp_source_timer_context));
	if (!c) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	const struct evdp_conf_itimer *conf = s->conf;
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

	return c;
}

void evdp_destroy_source_itimer_context(void *context)
{
	struct evdp_source_timer_context *c = context;

	free(c);
}

int evdp_source_itimer_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	struct evdp_source_timer_context *sc = s->context;

	EVDP_SLIST_PENDING_INSERT(q, s);
	sc->attached = 1;

	return 0;
}

void evdp_source_itimer_detach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	struct evdp_source_timer_context *c = s->context;
	if (c->attached && !s->pending) {
		const struct evdp_queue_context *qc = q->context;
		const struct evdp_conf_itimer *conf = s->conf;
		struct kevent e;
		EV_SET(&e, conf->ident, EVFILT_TIMER, EV_DISABLE | EV_DELETE, 0, 0, NULL);
		if (kevent(qc->fd, &e, 1, NULL, 0, NULL) == -1 && errno != ENOENT)
			neb_syslogl(LOG_ERR, "kevent: %m");
	}
	c->attached = 0;
}

neb_evdp_cb_ret_t evdp_source_itimer_handle(const struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	const struct kevent *e = ne->event;
	int64_t overrun = e->data;

	const struct evdp_conf_itimer *conf = ne->source->conf;
	if (conf->do_wakeup)
		ret = conf->do_wakeup(conf->ident, overrun, ne->source->udata);

	return ret;
}
