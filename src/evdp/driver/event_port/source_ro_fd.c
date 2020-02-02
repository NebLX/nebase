
#include <nebase/syslog.h>

#include "core.h"
#include "io_common.h"
#include "types.h"
#include "source_ro_fd.h"

#include <stdlib.h>
#include <poll.h>

int do_associate_ro_fd(const struct evdp_queue_context *qc, neb_evdp_source_t s)
{
	struct evdp_source_ro_fd_context *sc = s->context;
	const struct evdp_conf_ro_fd *conf = s->conf;
	if (port_associate(qc->fd, PORT_SOURCE_FD, conf->fd, POLLIN, s) == -1) {
		neb_syslogl(LOG_ERR, "port_associate: %m");
		return -1;
	}
	sc->associated = 1;
	return 0;
}

void *evdp_create_source_ro_fd_context(neb_evdp_source_t s)
{
	struct evdp_source_ro_fd_context *c = calloc(1, sizeof(struct evdp_source_ro_fd_context));
	if (!c) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	c->associated = 0;
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
	EVDP_SLIST_PENDING_INSERT(q, s);

	return 0;
}

void evdp_source_ro_fd_detach(neb_evdp_queue_t q, neb_evdp_source_t s, int to_close)
{
	const struct evdp_queue_context *qc = q->context;
	const struct evdp_conf_ro_fd *conf = s->conf;
	struct evdp_source_ro_fd_context *sc = s->context;

	if (to_close) {
		sc->associated = 0;
		return;
	}

	if (sc->associated) {
		if (port_dissociate(qc->fd, PORT_SOURCE_FD, conf->fd) == -1)
			neb_syslogl(LOG_ERR, "port_dissociate: %m");
		sc->associated = 0;
	}
}

neb_evdp_cb_ret_t evdp_source_ro_fd_handle(const struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	struct evdp_source_ro_fd_context *sc = ne->source->context;
	sc->associated = 0;

	const port_event_t *e = ne->event;

	const int fd = e->portev_object;
	const struct evdp_conf_ro_fd *conf = ne->source->conf;
	if (e->portev_events & POLLIN) {
		ret = conf->do_read(fd, ne->source->udata, &fd);
		if (ret != NEB_EVDP_CB_CONTINUE)
			return ret;
	}
	if (e->portev_events & POLLHUP) {
		ret = conf->do_hup(fd, ne->source->udata, &fd);
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
	if (ret == NEB_EVDP_CB_CONTINUE) {
		neb_evdp_queue_t q = ne->source->q_in_use;
		EVDP_SLIST_REMOVE(ne->source);
		q->stats.running--;
		EVDP_SLIST_PENDING_INSERT(q, ne->source);
	}

	return ret;
}
