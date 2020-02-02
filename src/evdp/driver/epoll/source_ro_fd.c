
#include <nebase/syslog.h>

#include "core.h"
#include "io_common.h"
#include "types.h"

#include <stdlib.h>

void *evdp_create_source_ro_fd_context(neb_evdp_source_t s)
{
	struct evdp_source_ro_fd_context *c = calloc(1, sizeof(struct evdp_source_ro_fd_context));
	if (!c) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	c->added = 0;
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

	sc->ctl_op = EPOLL_CTL_ADD;
	sc->ctl_event.data.ptr = s;
	sc->ctl_event.events = EPOLLIN;

	EVDP_SLIST_PENDING_INSERT(q, s);

	return 0;
}

void evdp_source_ro_fd_detach(neb_evdp_queue_t q, neb_evdp_source_t s, int to_close)
{
	const struct evdp_queue_context *qc = q->context;
	struct evdp_source_ro_fd_context *sc = s->context;
	const struct evdp_conf_ro_fd *conf = s->conf;

	if (to_close) {
		sc->added = 0;
		return;
	}

	if (sc->added) {
		if (epoll_ctl(qc->fd, EPOLL_CTL_DEL, conf->fd, NULL) == -1)
			neb_syslogl(LOG_ERR, "epoll_ctl: %m");
		sc->added = 0;
	}
}

neb_evdp_cb_ret_t evdp_source_ro_fd_handle(const struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	const struct epoll_event *e = ne->event;

	const struct evdp_conf_ro_fd *conf = ne->source->conf;
	if (e->events & EPOLLIN) {
		ret = conf->do_read(conf->fd, ne->source->udata, &conf->fd);
		if (ret != NEB_EVDP_CB_CONTINUE)
			return ret;
	}
	if (e->events & EPOLLHUP) {
		ret = conf->do_hup(conf->fd, ne->source->udata, &conf->fd);
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
