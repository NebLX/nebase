
#include <nebase/syslog.h>

#include "core.h"
#include "types.h"

#include <stdlib.h>
#include <errno.h>

void *evdp_create_queue_context(neb_evdp_queue_t q)
{
	struct evdp_queue_context *c = calloc(1, sizeof(struct evdp_queue_context));
	if (!c) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}
	c->id = 0; // must be initialized to 0

	c->ee = malloc(q->batch_size * sizeof(struct io_event));
	if (!c->ee) {
		neb_syslogl(LOG_ERR, "malloc: %m");
		evdp_destroy_queue_context(c);
		return NULL;
	}

	c->iocbv = malloc(q->batch_size * sizeof(struct iocb *));
	if (!c->iocbv) {
		neb_syslogl(LOG_ERR, "malloc: %m");
		evdp_destroy_queue_context(c);
		return NULL;
	}

	if (neb_aio_poll_create(q->batch_size, &c->id) == -1) {
		neb_syslogl(LOG_ERR, "aio_poll_create: %m");
		evdp_destroy_queue_context(c);
		return NULL;
	}

	return c;
}

void evdp_destroy_queue_context(void *context)
{
	struct evdp_queue_context *c = context;

	if (c->id)
		neb_aio_poll_destroy(c->id);
	if (c->iocbv)
		free(c->iocbv);
	if (c->ee)
		free(c->ee);
	free(c);
}

void evdp_queue_rm_pending_events(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	void *s_got = NULL, *s_to_rm = s;
	const struct evdp_queue_context *c = q->context;
	if (!c->ee)
		return;
	for (int i = q->current_event; i < q->nevents; i++) {
		struct io_event *e = c->ee + i;
		s_got = (neb_evdp_source_t)e->data;
		if (s_got == s_to_rm)
			e->data = 0;
	}
}

int evdp_queue_wait_events(neb_evdp_queue_t q, struct timespec *timeout)
{
	const struct evdp_queue_context *c = q->context;

	q->nevents = neb_aio_poll_wait(c->id, q->batch_size, c->ee, timeout);
	if (q->nevents == -1) {
		switch (errno) {
		case EINTR:
			q->nevents = 0;
			break;
		default:
			neb_syslogl(LOG_ERR, "aio_poll_wait: %m");
			return -1;
			break;
		}
	}
	return 0;
}

int evdp_queue_fetch_event(neb_evdp_queue_t q, struct neb_evdp_event *nee)
{
	const struct evdp_queue_context *c = q->context;

	struct io_event *e = c->ee + q->current_event;
	nee->event = e;
	nee->source = (neb_evdp_source_t)e->data;
	return 0;
}

void evdp_queue_finish_event(neb_evdp_queue_t q _nattr_unused, struct neb_evdp_event *nee _nattr_unused)
{
	return;
}

static int do_batch_flush(neb_evdp_queue_t q, int nr)
{
	const struct evdp_queue_context *qc = q->context;
	if (neb_aio_poll_submit(qc->id, nr, qc->iocbv) == -1) {
		neb_syslogl(LOG_ERR, "aio_poll_submit: %m");
		return -1;
	}
	q->stats.pending -= nr;
	q->stats.running += nr;
	for (int i = 0; i < nr; i++) {
		neb_evdp_source_t s = (neb_evdp_source_t)qc->iocbv[i]->aio_data;
		EVDP_SLIST_REMOVE(s);
		EVDP_SLIST_RUNNING_INSERT_NO_STATS(q, s);
		struct evdp_source_conext *sc = s->context;
		sc->submitted = 1;
	}
	return 0;
}

int evdp_queue_flush_pending_sources(neb_evdp_queue_t q)
{
	const struct evdp_queue_context *qc = q->context;
	int count = 0;
	neb_evdp_source_t last_s = NULL;
	for (neb_evdp_source_t s = q->pending_qs->next; s && s != last_s; ) {
		last_s = s;
		struct evdp_source_conext *sc = s->context;
		qc->iocbv[count++] = &sc->ctl_event;
		if (count >= q->batch_size) {
			if (do_batch_flush(q, count) != 0)
				return -1;
			count = 0;
			s = q->pending_qs->next;
		} else {
			s = s->next;
		}
	}
	if (count) {
		if (do_batch_flush(q, count) != 0)
			return -1;
	}
	return 0;
}
