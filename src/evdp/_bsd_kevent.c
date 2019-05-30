
#include <nebase/syslog.h>

#include "core.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/event.h>

struct evdp_queue_context {
	int fd;
	struct kevent *ee;
};

void *evdp_create_queue_context(neb_evdp_queue_t q)
{
	struct evdp_queue_context *c = calloc(1, sizeof(struct evdp_queue_context));
	if (!c) {
		neb_syslog(LOG_ERR, "malloc: %m");
		return NULL;
	}
	c->fd = -1;

	c->ee = malloc(q->batch_size * sizeof(struct kevent) * 2); //double for fd event
	if (!c->ee) {
		neb_syslog(LOG_ERR, "malloc: %m");
		evdp_destroy_queue_context(c);
		return NULL;
	}

	c->fd = kqueue();
	if (c->fd == -1) {
		neb_syslog(LOG_ERR, "kqueue: %m");
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
		struct kevent *e = c->ee + i;
		s_got = (neb_evdp_source_t)e->udata;
		if (s_got == s_to_rm)
# if defined(OS_NETBSD)
			e->udata = (intptr_t)NULL;
# else
			e->udata = NULL;
# endif
	}
}

int evdp_queue_wait_events(neb_evdp_queue_t q, struct timespec *timeout)
{
	struct evdp_queue_context *c = q->context;

	int readd_nevents = q->nevents; // re add events first
	q->nevents = kevent(c->fd, c->ee, readd_nevents, c->ee, q->batch_size, timeout);
	if (q->nevents == -1) {
		switch (errno) {
		case EINTR:
			q->nevents = 0;
			break;
		default:
			neb_syslog(LOG_ERR, "(kqueue %d)kevent: %m", c->fd);
			return -1;
			break;
		}
	}
	return 0;
}

int evdp_queue_fetch_event(neb_evdp_queue_t q, struct neb_evdp_event *nee)
{
	struct evdp_queue_context *c = q->context;

	struct kevent *e = c->ee + q->current_event;
	nee->event = e;
	nee->source = (neb_evdp_source_t)e->udata;
	if (e->flags & EV_ERROR) { // see return value of kevent
		neb_syslog_en(e->data, LOG_ERR, "kevent: %m");
		return -1;
	}
	return 0;
}
