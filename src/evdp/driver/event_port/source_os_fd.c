
#include <nebase/syslog.h>

#include "core.h"
#include "types.h"
#include "source_os_fd.h"

#include <stdlib.h>
#include <poll.h>
#include <errno.h>

int do_associate_os_fd(const struct evdp_queue_context *qc, neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	const struct evdp_conf_fd *conf = s->conf;
	if (port_associate(qc->fd, PORT_SOURCE_FD, conf->fd, sc->events, s) == -1) {
		neb_syslogl(LOG_ERR, "port_associate: %m");
		return -1;
	}
	sc->associated = 1;
	return 0;
}

static int do_disassociate_os_fd(const struct evdp_queue_context *qc, neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	const struct evdp_conf_fd *conf = s->conf;
	if (port_dissociate(qc->fd, PORT_SOURCE_FD, conf->fd) == -1) {
		if (errno == ENOENT)
			sc->associated = 0;
		neb_syslogl(LOG_ERR, "port_dissociate: %m");
		return -1;
	}
	sc->associated = 0;
	return 0;
}

void *evdp_create_source_os_fd_context(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *c = calloc(1, sizeof(struct evdp_source_os_fd_context));
	if (!c) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	c->associated = 0;
	s->pending = 0;

	return c;
}

void evdp_reset_source_os_fd_context(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *c = s->context;

	c->associated = 0;
	s->pending = 0;
	c->events = 0;
}

void evdp_destroy_source_os_fd_context(void *context)
{
	struct evdp_source_os_fd_context *c = context;

	free(c);
}

int evdp_source_os_fd_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	const struct evdp_source_os_fd_context *sc = s->context;

	if (sc->events & (POLLIN | POLLOUT)) {
		EVDP_SLIST_PENDING_INSERT(q, s);
	} else {
		EVDP_SLIST_RUNNING_INSERT(q, s);
	}

	return 0;
}

void evdp_source_os_fd_detach(neb_evdp_queue_t q, neb_evdp_source_t s, int to_close)
{
	const struct evdp_queue_context *qc = q->context;
	struct evdp_source_os_fd_context *sc = s->context;

	if (to_close) {
		sc->associated = 0;
		return;
	}

	if (sc->associated)
		do_disassociate_os_fd(qc, s);
}

neb_evdp_cb_ret_t evdp_source_os_fd_handle(const struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	neb_evdp_source_t s = ne->source;
	struct evdp_source_os_fd_context *sc = s->context;
	sc->associated = 0;

	const port_event_t *e = ne->event;

	const int fd = e->portev_object;
	const struct evdp_conf_fd *conf = s->conf;
	if ((e->portev_events & POLLIN) && conf->do_read) {
		sc->events &= ~POLLIN;
		ret = conf->do_read(fd, s->udata, &fd);
		if (ret != NEB_EVDP_CB_CONTINUE)
			return ret;
	}
	if (e->portev_events & POLLHUP) {
		ret = conf->do_hup(fd, s->udata, &fd);
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
	if ((e->portev_events & POLLOUT) && conf->do_write) {
		sc->events &= ~POLLOUT;
		ret = conf->do_write(fd, s->udata, &fd);
		if (ret != NEB_EVDP_CB_CONTINUE)
			return ret;
	}

	if (sc->events & (POLLIN | POLLOUT)) { // do pending if only handled one of them
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
		sc->events |= POLLIN;
	else
		sc->events &= ~POLLIN;
}

void evdp_source_os_fd_init_write(neb_evdp_source_t s, neb_evdp_io_handler_t rf)
{
	struct evdp_source_os_fd_context *sc = s->context;
	if (rf)
		sc->events |= POLLOUT;
	else
		sc->events &= ~POLLOUT;
}

int evdp_source_os_fd_reset_read(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	if (sc->associated) {
		if (sc->events & POLLIN)
			return 0;
		sc->events |= POLLIN;
		return do_associate_os_fd(s->q_in_use->context, s);
	} else {
		sc->events |= POLLIN;
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
	if (sc->associated) {
		if (sc->events & POLLOUT)
			return 0;
		sc->events |= POLLOUT;
		return do_associate_os_fd(s->q_in_use->context, s);
	} else {
		sc->events |= POLLOUT;
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
	if (sc->associated) {
		if (!(sc->events & POLLIN))
			return 0;
		sc->events ^= POLLIN;
		if (sc->events & POLLOUT)
			return 0;
		return do_disassociate_os_fd(s->q_in_use->context, s);
	} else if (s->pending) {
		sc->events &= ~POLLIN;
		if (sc->events & POLLOUT)
			return 0;
		neb_evdp_queue_t q = s->q_in_use;
		EVDP_SLIST_REMOVE(s);
		q->stats.pending--;
		EVDP_SLIST_RUNNING_INSERT(q, s);
	} else {
		sc->events &= ~POLLIN;
	}
	return 0;
}

int evdp_source_os_fd_unset_write(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	if (sc->associated) {
		if (!(sc->events & POLLOUT))
			return 0;
		sc->events ^= POLLOUT;
		if (sc->events & POLLIN)
			return 0;
		return do_disassociate_os_fd(s->q_in_use->context, s);
	} else if (s->pending) {
		sc->events &= ~POLLOUT;
		if (sc->events & POLLIN)
			return 0;
		neb_evdp_queue_t q = s->q_in_use;
		EVDP_SLIST_REMOVE(s);
		q->stats.pending--;
		EVDP_SLIST_RUNNING_INSERT(q, s);
	} else {
		sc->events &= ~POLLOUT;
	}
	return 0;
}
