
#include <nebase/syslog.h>

#include "core.h"
#include "types.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/timerfd.h>

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
	c->submitted = 0;
	c->in_action = 0;
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

	sc->ctl_event.aio_lio_opcode = IOCB_CMD_POLL;
	sc->ctl_event.aio_fildes = sc->fd;
	sc->ctl_event.aio_data = (uint64_t)s;
	sc->ctl_event.aio_buf = POLLIN;

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
	if (sc->submitted) {
		struct io_event e;
		if (neb_aio_poll_cancel(qc->id, &sc->ctl_event, &e) == -1)
			neb_syslogl(LOG_ERR, "aio_poll_cancel: %m");
		sc->submitted = 0;
	}
}

neb_evdp_cb_ret_t evdp_source_itimer_handle(const struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	struct evdp_source_timer_context *sc = ne->source->context;
	sc->submitted = 0;

	const struct io_event *e = ne->event;
	const struct iocb *iocb = (struct iocb *)e->obj;

	uint64_t overrun = 0;
	if (read(iocb->aio_fildes, &overrun, sizeof(overrun)) == -1) {
		neb_syslogl(LOG_ERR, "read: %m");
		return NEB_EVDP_CB_BREAK_ERR; // should not happen
	}

	const struct evdp_conf_itimer *conf = ne->source->conf;
	if (conf->do_wakeup)
		ret = conf->do_wakeup(conf->ident, overrun, ne->source->udata);
	if (ret == NEB_EVDP_CB_CONTINUE) {
		neb_evdp_queue_t q = ne->source->q_in_use;
		EVDP_SLIST_REMOVE(ne->source);
		q->stats.running--;
		EVDP_SLIST_PENDING_INSERT(ne->source->q_in_use, ne->source);
	}

	return ret;
}
