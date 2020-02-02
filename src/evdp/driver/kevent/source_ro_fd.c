
#include <nebase/syslog.h>

#include "core.h"
#include "io_common.h"
#include "types.h"

#include <stdlib.h>
#include <errno.h>

void *evdp_create_source_ro_fd_context(neb_evdp_source_t s)
{
	struct evdp_source_ro_fd_context *c = calloc(1, sizeof(struct evdp_source_ro_fd_context));
	if (!c) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

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

	EV_SET(&sc->ctl_event, conf->fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, s);

	EVDP_SLIST_PENDING_INSERT(q, s);

	return 0;
}

void evdp_source_ro_fd_detach(neb_evdp_queue_t q, neb_evdp_source_t s, int to_close)
{
	const struct evdp_queue_context *qc = q->context;
	struct evdp_source_ro_fd_context *sc = s->context;

	if (to_close)
		return;

	if (!s->pending) {
		sc->ctl_event.flags = EV_DISABLE | EV_DELETE;
		if (kevent(qc->fd, &sc->ctl_event, 1, NULL, 0, NULL) == -1 && errno != ENOENT)
			neb_syslogl(LOG_ERR, "kevent: %m");
	}
}

neb_evdp_cb_ret_t evdp_source_ro_fd_handle(const struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	const struct kevent *e = ne->event;

	const struct evdp_conf_ro_fd *conf = ne->source->conf;
	if (e->filter == EVFILT_READ) {
		ret = conf->do_read(e->ident, ne->source->udata, e);
		if (ret != NEB_EVDP_CB_CONTINUE)
			return ret;
	}
	if (e->flags & EV_EOF) {
		ret = conf->do_hup(e->ident, ne->source->udata, e);
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
