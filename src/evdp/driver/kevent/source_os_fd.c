
#include <nebase/syslog.h>

#include "core.h"
#include "io_base.h"
#include "types.h"

#include <stdlib.h>
#include <errno.h>

void *evdp_create_source_os_fd_context(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *c = calloc(1, sizeof(struct evdp_source_os_fd_context));
	if (!c) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	s->pending = 0;
	c->rd.added = 0;
	c->rd.to_add = 0;
	c->wr.added = 0;
	c->wr.to_add = 0;

	return c;
}

void evdp_reset_source_os_fd_context(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *c = s->context;

	s->pending = 0;
	c->rd.added = 0;
	c->rd.to_add = 0;
	c->wr.added = 0;
	c->wr.to_add = 0;
}

void evdp_destroy_source_os_fd_context(void *context)
{
	struct evdp_source_os_fd_context *c = context;

	free(c);
}

int evdp_source_os_fd_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	const struct evdp_conf_fd *conf = s->conf;

	EV_SET(&sc->rd.ctl_event, conf->fd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_ONESHOT, 0, 0, s);
	EV_SET(&sc->wr.ctl_event, conf->fd, EVFILT_WRITE, EV_ADD | EV_ENABLE | EV_ONESHOT, 0, 0, s);

	if (sc->rd.to_add || sc->wr.to_add) {
		EVDP_SLIST_PENDING_INSERT(q, s);
		sc->stats_updated = 0;
	} else {
		EVDP_SLIST_RUNNING_INSERT(q, s);
	}

	return 0;
}

static int do_del_os_fd_rd(const struct evdp_queue_context *qc, struct evdp_source_os_fd_context *sc)
{
	sc->rd.ctl_event.flags = EV_DISABLE | EV_DELETE;
	if (kevent(qc->fd, &sc->rd.ctl_event, 1, NULL, 0, NULL) == -1 && errno != ENOENT) {
		neb_syslogl(LOG_ERR, "kevent: %m");
		return -1;
	}
	sc->rd.added = 0;
	return 0;
}

static int do_del_os_fd_wr(const struct evdp_queue_context *qc, struct evdp_source_os_fd_context *sc)
{
	sc->wr.ctl_event.flags = EV_DISABLE | EV_DELETE;
	if (kevent(qc->fd, &sc->wr.ctl_event, 1, NULL, 0, NULL) == -1 && errno != ENOENT) {
		neb_syslogl(LOG_ERR, "kevent: %m");
		return -1;
	}
	sc->wr.added = 0;
	return 0;
}

void evdp_source_os_fd_detach(neb_evdp_queue_t q, neb_evdp_source_t s, int to_close)
{
	const struct evdp_queue_context *qc = q->context;
	struct evdp_source_os_fd_context *sc = s->context;

	if (to_close) {
		sc->rd.added = 0;
		sc->wr.added = 0;
		return;
	}

	if (sc->rd.added)
		do_del_os_fd_rd(qc, sc);
	if (sc->wr.added)
		do_del_os_fd_wr(qc, sc);
}

neb_evdp_cb_ret_t evdp_source_os_fd_handle(const struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	struct evdp_source_os_fd_context *sc = ne->source->context;

	const struct kevent *e = ne->event;

	const struct evdp_conf_fd *conf = ne->source->conf;
	switch (e->filter) {
	case EVFILT_READ:
		sc->rd.added = 0;
		// sc->rd.to_add = 0;

		ret = conf->do_read(e->ident, ne->source->udata, e);
		if (ret != NEB_EVDP_CB_CONTINUE)
			return ret;

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
		break;
	case EVFILT_WRITE:
		sc->wr.added = 0;
		// sc->wr.to_add = 0;

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

		ret = conf->do_write(e->ident, ne->source->udata, e);
		if (ret != NEB_EVDP_CB_CONTINUE)
			return ret;
		break;
	default:
		neb_syslog(LOG_ERR, "Invalid filter type %d for os_fd", e->filter);
		return NEB_EVDP_CB_BREAK_ERR;
		break;
	}

	return ret;
}

void evdp_source_os_fd_init_read(neb_evdp_source_t s, neb_evdp_io_handler_t rf)
{
	struct evdp_source_os_fd_context *sc = s->context;
	if (rf)
		sc->rd.to_add = 1;
	else
		sc->rd.to_add = 0;
}

void evdp_source_os_fd_init_write(neb_evdp_source_t s, neb_evdp_io_handler_t rf)
{
	struct evdp_source_os_fd_context *sc = s->context;
	if (rf)
		sc->wr.to_add = 1;
	else
		sc->wr.to_add = 0;
}

int evdp_source_os_fd_reset_read(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	if (sc->rd.added) {
		return 0;
	} else {
		sc->rd.to_add = 1;
		sc->rd.ctl_event.flags = EV_ADD | EV_ENABLE | EV_ONESHOT;
		if (!s->pending) { // Make sure add to pending
			neb_evdp_queue_t q = s->q_in_use;
			EVDP_SLIST_REMOVE(s);
			q->stats.running--;
			EVDP_SLIST_PENDING_INSERT(q, s);
		}
	}
	return 0;
}

int evdp_source_os_fd_reset_write(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	if (sc->wr.added) {
		return 0;
	} else {
		sc->wr.to_add = 1;
		sc->wr.ctl_event.flags = EV_ADD | EV_ENABLE | EV_ONESHOT;
		if (!s->pending) { // Make sure add to pending
			neb_evdp_queue_t q = s->q_in_use;
			EVDP_SLIST_REMOVE(s);
			q->stats.running--;
			EVDP_SLIST_PENDING_INSERT(q, s);
		}
	}
	return 0;
}

int evdp_source_os_fd_unset_read(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	if (sc->rd.added) {
		return do_del_os_fd_rd(s->q_in_use->context, sc);
	} else if (s->pending) {
		sc->rd.to_add = 0;
		if (sc->wr.to_add)
			return 0;
		neb_evdp_queue_t q = s->q_in_use;
		EVDP_SLIST_REMOVE(s);
		q->stats.pending--;
		EVDP_SLIST_RUNNING_INSERT(q, s);
	} else {
		sc->rd.to_add = 0;
	}
	return 0;
}

int evdp_source_os_fd_unset_write(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	if (sc->wr.added) {
		return do_del_os_fd_wr(s->q_in_use->context, sc);
	} else if (s->pending) {
		sc->wr.to_add = 0;
		if (sc->rd.to_add)
			return 0;
		neb_evdp_queue_t q = s->q_in_use;
		EVDP_SLIST_REMOVE(s);
		q->stats.pending--;
		EVDP_SLIST_RUNNING_INSERT(q, s);
	} else {
		sc->wr.to_add = 0;
	}
	return 0;
}
