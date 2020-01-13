
#include <nebase/syslog.h>

#include "core.h"
#include "types.h"

#include <stdlib.h>
#include <poll.h>

void *evdp_create_source_os_fd_context(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *c = calloc(1, sizeof(struct evdp_source_os_fd_context));
	if (!c) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	c->submitted = 0;
	s->pending = 0;

	return c;
}

void evdp_reset_source_os_fd_context(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *c = s->context;

	c->submitted = 0;
	s->pending = 0;
	c->ctl_event = 0;
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

	sc->fd = conf->fd;
	// event type is dynamic

	if (sc->ctl_event & (POLLIN | POLLOUT)) {
		EVDP_SLIST_PENDING_INSERT(q, s);
	} else {
		EVDP_SLIST_RUNNING_INSERT(q, s);
	}

	return 0;
}

static int do_submit_os_fd(struct evdp_queue_context *qc, neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	struct io_uring_sqe *sqe = io_uring_get_sqe(&qc->ring);
	if (!sqe) {
		neb_syslog(LOG_CRIT, "no sqe left");
		return -1;
	}
	io_uring_prep_poll_add(sqe, sc->fd, sc->ctl_event);
	io_uring_sqe_set_data(sqe, s);
	int ret = io_uring_submit(&qc->ring);
	if (ret < 0) {
		neb_syslogl(LOG_ERR, "io_uring_submit: %m");
		return -1;
	}
	sc->submitted = 1;
	return 0;
}

static int do_cancel_os_fd(struct evdp_queue_context *qc, neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	struct io_uring_sqe *sqe = io_uring_get_sqe(&qc->ring);
	if (!sqe) {
		neb_syslog(LOG_CRIT, "no sqe left");
		return -1;
	}
	io_uring_prep_poll_remove(sqe, s);
	int ret = io_uring_submit(&qc->ring);
	if (ret < 0) {
		neb_syslogl(LOG_ERR, "io_uring_submit: %m");
		return -1;
	}
	sc->submitted = 0;
	return 0;
}

void evdp_source_os_fd_detach(neb_evdp_queue_t q, neb_evdp_source_t s, int to_close)
{
	struct evdp_queue_context *qc = q->context;
	struct evdp_source_os_fd_context *sc = s->context;

	if (to_close) {
		sc->submitted = 0;
		return;
	}

	if (sc->submitted)
		do_cancel_os_fd(qc, s);
}

neb_evdp_cb_ret_t evdp_source_os_fd_handle(const struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	neb_evdp_source_t s = ne->source;
	struct evdp_source_os_fd_context *sc = s->context;
	sc->submitted = 0;

	const struct io_uring_cqe *e = ne->event;

	const int fd = sc->fd;
	const struct evdp_conf_fd *conf = s->conf;
	if ((e->res & POLLIN) && conf->do_read) {
		sc->ctl_event &= ~POLLIN;
		ret = conf->do_read(fd, s->udata, &fd);
		if (ret != NEB_EVDP_CB_CONTINUE)
			return ret;
	}
	if (e->res & POLLHUP) {
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
	if ((e->res & POLLOUT) && conf->do_write) {
		sc->ctl_event &= ~POLLOUT;
		ret = conf->do_write(fd, s->udata, &fd);
		if (ret != NEB_EVDP_CB_CONTINUE)
			return ret;
	}

	if (sc->ctl_event & (POLLIN | POLLOUT)) { // do pending if only handled one of them
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
		sc->ctl_event |= POLLIN;
	else
		sc->ctl_event &= ~POLLIN;
}

void evdp_source_os_fd_init_write(neb_evdp_source_t s, neb_evdp_io_handler_t rf)
{
	struct evdp_source_os_fd_context *sc = s->context;
	if (rf)
		sc->ctl_event |= POLLOUT;
	else
		sc->ctl_event &= ~POLLOUT;
}

int evdp_source_os_fd_reset_read(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	if (sc->submitted) {
		if (sc->ctl_event & POLLIN)
			return 0;
		sc->ctl_event |= POLLIN;
		return do_submit_os_fd(s->q_in_use->context, s);
	} else {
		sc->ctl_event |= POLLIN;
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
	if (sc->submitted) {
		if (sc->ctl_event & POLLOUT)
			return 0;
		sc->ctl_event |= POLLOUT;
		return do_submit_os_fd(s->q_in_use->context, s);
	} else {
		sc->ctl_event |= POLLOUT;
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
	if (sc->submitted) {
		if (!(sc->ctl_event & POLLIN))
			return 0;
		sc->ctl_event ^= POLLIN;
		if (sc->ctl_event & POLLOUT)
			return 0;
		return do_cancel_os_fd(s->q_in_use->context, s);
	} else if (s->pending) {
		sc->ctl_event &= ~POLLIN;
		if (sc->ctl_event & POLLOUT)
			return 0;
		neb_evdp_queue_t q = s->q_in_use;
		EVDP_SLIST_REMOVE(s);
		q->stats.pending--;
		EVDP_SLIST_RUNNING_INSERT(q, s);
	} else {
		sc->ctl_event &= ~POLLIN;
	}
	return 0;
}

int evdp_source_os_fd_unset_write(neb_evdp_source_t s)
{
	struct evdp_source_os_fd_context *sc = s->context;
	if (sc->submitted) {
		if (!(sc->ctl_event & POLLOUT))
			return 0;
		sc->ctl_event ^= POLLOUT;
		if (sc->ctl_event & POLLIN)
			return 0;
		return do_cancel_os_fd(s->q_in_use->context, s);
	} else if (s->pending) {
		sc->ctl_event &= ~POLLOUT;
		if (sc->ctl_event & POLLIN)
			return 0;
		neb_evdp_queue_t q = s->q_in_use;
		EVDP_SLIST_REMOVE(s);
		q->stats.pending--;
		EVDP_SLIST_RUNNING_INSERT(q, s);
	} else {
		sc->ctl_event &= ~POLLOUT;
	}
	return 0;
}
