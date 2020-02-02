
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

	c->added = 0;
	s->pending = 0;

	return c;
}

void evdp_reset_source_os_fd_context(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *c = s->context;

	c->added = 0;
	s->pending = 0;
	c->ctl_event.events = 0;
}

void evdp_destroy_source_os_fd_context(void *context)
{
	struct evdp_source_os_fd_context *c = context;

	free(c);
}

int evdp_source_os_fd_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;

	sc->ctl_op = EPOLL_CTL_ADD;
	sc->ctl_event.data.ptr = s;
	sc->ctl_event.events |= EPOLLONESHOT; // the real event is dynamic

	if (sc->ctl_event.events & (EPOLLIN | EPOLLOUT)) {
		EVDP_SLIST_PENDING_INSERT(q, s);
	} else {
		EVDP_SLIST_RUNNING_INSERT(q, s);
	}

	return 0;
}

static int do_ctl_os_fd(const struct evdp_queue_context *qc, neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	const struct evdp_conf_fd *conf = s->conf;
	if (epoll_ctl(qc->fd, sc->ctl_op, conf->fd, &sc->ctl_event) == -1) {
		neb_syslogl(LOG_ERR, "epoll_ctl(op:%d): %m", sc->ctl_op);
		return -1;
	}
	sc->added = 1;
	return 0;
}

static int do_del_os_fd(const struct evdp_queue_context *qc, neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	const struct evdp_conf_fd *conf = s->conf;
	if (epoll_ctl(qc->fd, EPOLL_CTL_DEL, conf->fd, NULL) == -1) {
		if (errno == ENOENT)
			sc->added = 0;
		neb_syslogl(LOG_ERR, "epoll_ctl: %m");
		return -1;
	}
	sc->added = 0;
	sc->ctl_op = EPOLL_CTL_ADD;
	return 0;
}

void evdp_source_os_fd_detach(neb_evdp_queue_t q, neb_evdp_source_t s, int to_close)
{
	const struct evdp_queue_context *qc = q->context;
	struct evdp_source_os_fd_context *sc = s->context;

	if (to_close) {
		sc->added = 0;
		return;
	}

	if (sc->added)
		do_del_os_fd(qc, s);
}

neb_evdp_cb_ret_t evdp_source_os_fd_handle(const struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	neb_evdp_source_t s = ne->source;
	struct evdp_source_os_fd_context *sc = s->context;
	sc->added = 0;
	sc->ctl_op = EPOLL_CTL_MOD;

	const struct epoll_event *e = ne->event;

	const struct evdp_conf_fd *conf = s->conf;
	if ((e->events & EPOLLIN) && conf->do_read) {
		sc->ctl_event.events &= ~EPOLLIN;
		ret = conf->do_read(conf->fd, s->udata, &conf->fd);
		if (ret != NEB_EVDP_CB_CONTINUE)
			return ret;
	}
	if (e->events & EPOLLHUP) {
		ret = conf->do_hup(conf->fd, s->udata, &conf->fd);
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
	if ((e->events & EPOLLOUT) && conf->do_write) {
		sc->ctl_event.events &= ~EPOLLOUT;
		ret = conf->do_write(conf->fd, s->udata, &conf->fd);
		if (ret != NEB_EVDP_CB_CONTINUE)
			return ret;
	}

	if (sc->ctl_event.events & (EPOLLIN | EPOLLOUT)) { // do pending if only handled one of them
		neb_evdp_queue_t q = s->q_in_use;
		EVDP_SLIST_REMOVE(s);
		q->stats.running--;
		EVDP_SLIST_PENDING_INSERT(q, s);
	}

	return ret;
}

void evdp_source_os_fd_init_read(neb_evdp_source_t s, neb_evdp_io_handler_t rf)
{
	struct evdp_source_os_fd_context *sc = s->context;
	if (rf)
		sc->ctl_event.events |= EPOLLIN;
	else
		sc->ctl_event.events &= ~EPOLLIN;
}

void evdp_source_os_fd_init_write(neb_evdp_source_t s, neb_evdp_io_handler_t rf)
{
	struct evdp_source_os_fd_context *sc = s->context;
	if (rf)
		sc->ctl_event.events |= EPOLLOUT;
	else
		sc->ctl_event.events &= ~EPOLLOUT;
}

int evdp_source_os_fd_reset_read(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	if (sc->added) {
		if (sc->ctl_event.events & EPOLLIN)
			return 0;
		sc->ctl_event.events |= EPOLLIN;
		return do_ctl_os_fd(s->q_in_use->context, s);
	} else {
		sc->ctl_event.events |= EPOLLIN;
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
	if (sc->added) {
		if (sc->ctl_event.events & EPOLLOUT)
			return 0;
		sc->ctl_event.events |= EPOLLOUT;
		return do_ctl_os_fd(s->q_in_use->context, s);
	} else {
		sc->ctl_event.events |= EPOLLOUT;
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
	if (sc->added) {
		if (!(sc->ctl_event.events & EPOLLIN))
			return 0;
		sc->ctl_event.events ^= EPOLLIN;
		if (sc->ctl_event.events & EPOLLOUT)
			return 0;
		return do_del_os_fd(s->q_in_use->context, s);
	} else if (s->pending) {
		sc->ctl_event.events &= ~EPOLLIN;
		if (sc->ctl_event.events & EPOLLOUT)
			return 0;
		neb_evdp_queue_t q = s->q_in_use;
		EVDP_SLIST_REMOVE(s);
		q->stats.pending--;
		EVDP_SLIST_RUNNING_INSERT(q, s);
	} else {
		sc->ctl_event.events &= ~EPOLLIN;
	}
	return 0;
}

int evdp_source_os_fd_unset_write(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	if (sc->added) {
		if (!(sc->ctl_event.events & EPOLLOUT))
			return 0;
		sc->ctl_event.events ^= EPOLLOUT;
		if (sc->ctl_event.events & EPOLLIN)
			return 0;
		return do_del_os_fd(s->q_in_use->context, s);
	} else if (s->pending) {
		sc->ctl_event.events &= ~EPOLLOUT;
		if (sc->ctl_event.events & EPOLLIN)
			return 0;
		neb_evdp_queue_t q = s->q_in_use;
		EVDP_SLIST_REMOVE(s);
		q->stats.pending--;
		EVDP_SLIST_RUNNING_INSERT(q, s);
	} else {
		sc->ctl_event.events &= ~EPOLLOUT;
	}
	return 0;
}
