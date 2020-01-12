
#include <nebase/syslog.h>

#include "core.h"
#include "types.h"

#include "source_ro_fd.h"
#include "source_os_fd.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

void *evdp_create_queue_context(neb_evdp_queue_t q)
{
	struct evdp_queue_context *c = calloc(1, sizeof(struct evdp_queue_context));
	if (!c) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}
	c->fd = -1;

	c->ee = malloc(q->batch_size * sizeof(port_event_t));
	if (!c->ee) {
		neb_syslogl(LOG_ERR, "malloc: %m");
		evdp_destroy_queue_context(c);
		return NULL;
	}

	c->fd = port_create();
	if (c->fd == -1) {
		neb_syslogl(LOG_ERR, "port_create: %m");
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
	const struct evdp_queue_context *c = q->context;
	if (!c->ee)
		return;
	for (int i = q->current_event; i < q->nevents; i++) {
		port_event_t *e = c->ee + i;
		s_got = e->portev_user;
		if (s_got == s_to_rm)
			e->portev_user = NULL;
	}
}

int evdp_queue_wait_events(neb_evdp_queue_t q, int timeout_msec)
{
	const struct evdp_queue_context *c = q->context;

	struct timespec ts;
	struct timespec *timeout = NULL;
	if (timeout_msec != -1) {
		ts.tv_sec = timeout_msec / 1000;
		ts.tv_nsec = (timeout_msec % 1000) * 1000000;
		timeout = &ts;
	}

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
			neb_syslogl(LOG_ERR, "port_getn: %m");
			return -1;
			break;
		}
	}
	q->nevents = nget;
	return 0;
}

int evdp_queue_fetch_event(neb_evdp_queue_t q, struct neb_evdp_event *nee)
{
	const struct evdp_queue_context *c = q->context;

	port_event_t *e = c->ee + q->current_event;
	nee->event = e;
	nee->source = e->portev_user;
	return 0;
}

int evdp_queue_flush_pending_sources(neb_evdp_queue_t q)
{
	const struct evdp_queue_context *qc = q->context;
	int count = 0;
	for (neb_evdp_source_t s = q->pending_qs->next; s; s = q->pending_qs->next) {
		int ret = 0;
		switch (s->type) {
		case EVDP_SOURCE_RO_FD:
			ret = do_associate_ro_fd(qc, s);
			break;
		case EVDP_SOURCE_OS_FD:
			ret = do_associate_os_fd(qc, s);
			break;
		case EVDP_SOURCE_LT_FD:
			break;
		// TODO add other source type here
		default:
			neb_syslog(LOG_ERR, "Unsupported associate source type %d", s->type);
			ret = -1;
			break;
		}
		if (ret)
			return ret;
		EVDP_SLIST_REMOVE(s);
		EVDP_SLIST_RUNNING_INSERT_NO_STATS(q, s);
		count++;
	}
	if (count) {
		q->stats.pending -= count;
		q->stats.running += count;
	}
	return 0;
}
