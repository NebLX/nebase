
#include <nebase/syslog.h>

#include "core.h"
#include "io_base.h"
#include "types.h"

#include <stdlib.h>
#include <poll.h>

void *evdp_create_source_ro_fd_context(neb_evdp_source_t s)
{
	struct evdp_source_ro_fd_context *c = calloc(1, sizeof(struct evdp_source_ro_fd_context));
	if (!c) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	c->submitted = 0;
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
	const struct evdp_conf_ro_fd *conf = s->conf;

	sc->ctl_event.aio_lio_opcode = IOCB_CMD_POLL;
	sc->ctl_event.aio_fildes = conf->fd;
	sc->ctl_event.aio_data = (uint64_t)s;
	sc->ctl_event.aio_buf = POLLIN;

	EVDP_SLIST_PENDING_INSERT(q, s);

	return 0;
}

void evdp_source_ro_fd_detach(neb_evdp_queue_t q, neb_evdp_source_t s, int to_close)
{
	const struct evdp_queue_context *qc = q->context;
	struct evdp_source_ro_fd_context *sc = s->context;

	if (to_close) {
		sc->submitted = 0;
		return;
	}

	if (sc->submitted) {
		struct io_event e;
		if (neb_aio_poll_cancel(qc->id, &sc->ctl_event, &e) == -1)
			neb_syslogl(LOG_ERR, "aio_poll_cancel: %m");
		sc->submitted = 0;
	}
}

neb_evdp_cb_ret_t evdp_source_ro_fd_handle(const struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	struct evdp_source_ro_fd_context *sc = ne->source->context;
	sc->submitted = 0;

	const struct io_event *e = ne->event;
	const struct iocb *iocb = (struct iocb *)e->obj;

	const int fd = iocb->aio_fildes;
	const struct evdp_conf_ro_fd *conf = ne->source->conf;
	if (e->res & POLLIN) {
		ret = conf->do_read(fd, ne->source->udata, &fd);
		if (ret != NEB_EVDP_CB_CONTINUE)
			return ret;
	}
	if (e->res & POLLHUP) {
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
