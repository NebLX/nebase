
#include <nebase/syslog.h>

#include "core.h"
#include "_aio_poll.h"

#include <stdlib.h>
#include <errno.h>

struct evdp_queue_context {
	aio_context_t id;
	struct io_event *ee;
};

void *evdp_create_queue_context(neb_evdp_queue_t q)
{
	struct evdp_queue_context *c = calloc(1, sizeof(struct evdp_queue_context));
	if (!c) {
		neb_syslog(LOG_ERR, "malloc: %m");
		return NULL;
	}
	c->id = 0; // must be initialized to 0

	c->ee = malloc(q->batch_size * sizeof(struct io_event));
	if (!c->ee) {
		neb_syslog(LOG_ERR, "malloc: %m");
		evdp_destroy_queue_context(c);
		return NULL;
	}

	if (neb_aio_poll_create(q->batch_size, &c->id) == -1) {
		neb_syslog(LOG_ERR, "aio_poll_create: %m");
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
	if (c->ee)
		free(c->ee);
	free(c);
}

void evdp_queue_rm_pending_events(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	void *s_got = NULL, *s_to_rm = s;
	struct evdp_queue_context *c = q->context;
	if (!c->ee)
		return;
	for (int i = q->current_event; i < q->nevents; i++) {
		struct io_event *e = c->ee + i;
		s_got = (void *)e->data;
		if (s_got == s_to_rm)
			e->data = 0;
	}
}

int evdp_queue_wait_events(neb_evdp_queue_t q, struct timespec *timeout)
{
	struct evdp_queue_context *c = q->context;

	q->nevents = neb_aio_poll_wait(c->id, q->batch_size, c->ee, timeout);
	if (q->nevents == -1) {
		switch (errno) {
		case EINTR:
			q->nevents = 0;
			break;
		default:
			neb_syslog(LOG_ERR, "(aio %lu)aio_poll_wait: %m", c->id);
			return -1;
			break;
		}
	}
	return 0;
}

int evdp_queue_fetch_event(neb_evdp_queue_t q, struct neb_evdp_event *nee)
{
	struct evdp_queue_context *c = q->context;

	struct io_event *e = c->ee + q->current_event;
	nee->event = e;
	nee->source = (neb_evdp_source_t)e->data;
	return 0;
}
