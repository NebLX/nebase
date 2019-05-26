
#include <nebase/syslog.h>

#include "core.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <port.h>

struct evdp_queue_context {
	int fd;
	port_event_t *ee;
};

void *evdp_create_queue_context(neb_evdp_queue_t q)
{
	struct evdp_queue_context *c = calloc(1, sizeof(struct evdp_queue_context));
	if (!c) {
		neb_syslog(LOG_ERR, "malloc: %m");
		return NULL;
	}
	c->fd = -1;

	c->ee = malloc(q->batch_size * sizeof(port_event_t));
	if (!c->ee) {
		neb_syslog(LOG_ERR, "malloc: %m");
		evdp_destroy_queue_context(c);
		return NULL;
	}

	c->fd = port_create();
	if (c->fd == -1) {
		neb_syslog(LOG_ERR, "port_create: %m");
		evdp_destroy_queue_context(c);
		return NULL;
	}

	return c;
}

void evdp_destroy_queue_context(void *context)
{
	struct evdp_queue_context *c = context;

	if (c->fd >= 0)
		close(c->fd);
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
		port_event_t *e = c->ee + i;
		s_got = e->portev_user;
		if (s_got == s_to_rm)
			e->portev_user = NULL;
	}
}

int evdp_queue_wait_events(neb_evdp_queue_t q, struct timespec *timeout)
{
	struct evdp_queue_context *c = q->context;

	uint_t nget = 1;
	if (port_getn(c->fd, c->ee, q->batch_size, &nget, timeout) == -1) {
		switch(errno) {
		case EINTR:
			q->nevents = 0;
			return 0;
			break;
		case ETIME:
			break;
		default:
			neb_syslog(LOG_ERR, "(port %d)port_getn: %m", c->fd);
			return -1;
			break;
		}
	}
	q->nevents = nget;
	return 0;
}

int evdp_queue_fetch_event(neb_evdp_queue_t q, struct neb_evdp_event *nee)
{
	struct evdp_queue_context *c = q->context;

	port_event_t *e = c->ee + q->current_event;
	nee->event = e;
	nee->source = e->portev_user;
	return 0;
}
