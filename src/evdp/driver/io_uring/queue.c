
#include <nebase/syslog.h>

#include "core.h"
#include "types.h"

#include <stdlib.h>
#include <errno.h>

#include <liburing.h>

void *evdp_create_queue_context(neb_evdp_queue_t q)
{
	struct evdp_queue_context *c = calloc(1, sizeof(struct evdp_queue_context));
	if (!c) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	int ret = io_uring_queue_init(4096, &c->ring, 0);
	if (ret < 0) {
		neb_syslogl_en(-ret, LOG_ERR, "io_uring_queue_init: %m");
		evdp_destroy_queue_context(c);
		return NULL;
	}
	c->ring_ok = 1;

	c->cqe = malloc(q->batch_size * sizeof(struct io_uring_cqe *));
	if (!c->cqe) {
		neb_syslogl(LOG_ERR, "malloc: %m");
		evdp_destroy_queue_context(c);
		return NULL;
	}

	return c;
}

void evdp_destroy_queue_context(void *context)
{
	struct evdp_queue_context *c = context;

	if (c->cqe)
		free(c->cqe);
	if (c->ring_ok)
		io_uring_queue_exit(&c->ring);
	free(c);
}

void evdp_queue_rm_pending_events(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	void *s_got = NULL, *s_to_rm = s;
	const struct evdp_queue_context *c = q->context;
	if (!c->cqe)
		return;
	for (int i = q->current_event; i < q->nevents; i++) {
		struct io_uring_cqe *e = c->cqe[i];
		s_got = (neb_evdp_source_t)e->user_data;
		if (s_got == s_to_rm)
			e->user_data = 0;
	}
}

int evdp_queue_wait_events(neb_evdp_queue_t q, int timeout_msec)
{
	struct evdp_queue_context *c = q->context;

	// try batch first
	q->nevents = io_uring_peek_batch_cqe(&c->ring, c->cqe, q->batch_size);
	if (q->nevents > 0)
		return 0;

	// no event yet, wait till timeout
	struct __kernel_timespec ts;
	struct __kernel_timespec *timeout = NULL;
	if (timeout_msec != -1) {
		ts.tv_sec = timeout_msec / 1000;
		ts.tv_nsec = (timeout_msec % 1000) * 1000000;
		timeout = &ts;
	}

	struct io_uring_cqe *cqe = NULL;
	int ret = io_uring_wait_cqe_timeout(&c->ring, &cqe, timeout);
	if (ret < 0) {
		switch (-ret) {
		case EAGAIN:
		case EINTR:
		case ETIME:
			return 0;
		default:
			neb_syslogl_en(-ret, LOG_ERR, "io_uring_wait_cqe_timeout: %m");
			return -1;
			break;
		}
	}

	if (cqe) // try batch again
		q->nevents = io_uring_peek_batch_cqe(&c->ring, c->cqe, q->batch_size);

	return 0;
}

int evdp_queue_fetch_event(neb_evdp_queue_t q, struct neb_evdp_event *nee)
{
	const struct evdp_queue_context *c = q->context;

	struct io_uring_cqe *e = c->cqe[q->current_event];
	nee->event = e;
	nee->source = (neb_evdp_source_t)e->user_data;
	return 0;
}

void evdp_queue_finish_event(neb_evdp_queue_t q, struct neb_evdp_event *nee)
{
	struct evdp_queue_context *qc = q->context;
	io_uring_cqe_seen(&qc->ring, nee->event);
}

int evdp_queue_flush_pending_sources(neb_evdp_queue_t q)
{
	struct evdp_queue_context *qc = q->context;
	int count = 0;
	for (neb_evdp_source_t s = q->pending_qs->next; s; s = q->pending_qs->next) {
		struct evdp_source_context *sc = s->context;
		struct io_uring_sqe *sqe = io_uring_get_sqe(&qc->ring);
		if (!sqe) { // FIXME
			neb_syslog(LOG_CRIT, "no sqe available");
			return -1;
		}

		// FIXME we may want to prep for other events
		io_uring_prep_poll_add(sqe, sc->fd, sc->ctl_event);
		io_uring_sqe_set_data(sqe, s);

		EVDP_SLIST_REMOVE(s);
		EVDP_SLIST_RUNNING_INSERT_NO_STATS(q, s);
		count++;
	}
	if (count) {
		int ret = io_uring_submit(&qc->ring);
		if (ret < 0) {
			neb_syslogl_en(-ret, LOG_ERR, "io_uring_submit: %m");
			return -1;
		}

		q->stats.pending -= count;
		q->stats.running += count;
	}

	return 0;
}
