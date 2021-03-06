
#include <nebase/syslog.h>

#include "core.h"
#include "io_base.h"
#include "types.h"
#include "helper.h"

#include <stdlib.h>
#include <poll.h>

void *evdp_create_source_ro_fd_context(neb_evdp_source_t s)
{
	struct evdp_source_ro_fd_context *c = calloc(1, sizeof(struct evdp_source_ro_fd_context));
	if (!c) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	c->multishot = 1;
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

	sc->ctl_event = POLLIN;
	sc->fd = conf->fd;

	EVDP_SLIST_PENDING_INSERT(q, s);

	return 0;
}

void evdp_source_ro_fd_detach(neb_evdp_queue_t q, neb_evdp_source_t s, int to_close)
{
	struct evdp_queue_context *qc = q->context;
	struct evdp_source_ro_fd_context *sc = s->context;

	if (to_close) {
		sc->submitted = 0;
		return;
	}

	if (sc->submitted) {
		if (neb_io_uring_cancel_fd(qc, s) != 0)
			neb_syslog(LOG_ERR, "failed to cancel ro_fd source");
		sc->submitted = 0;
	}
}

neb_evdp_cb_ret_t evdp_source_ro_fd_handle(const struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	struct evdp_source_ro_fd_context *sc = ne->source->context;
	sc->submitted = 0;

	const struct io_uring_cqe *e = ne->event;

	const int fd = sc->fd;
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

	return ret;
}
