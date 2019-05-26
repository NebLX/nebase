
#include <nebase/syslog.h>

#include "core.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>

struct evdp_queue_context {
	int fd;
	struct epoll_event *ee;
};

void *evdp_create_queue_context(neb_evdp_queue_t q)
{
	struct evdp_queue_context *c = calloc(1, sizeof(struct evdp_queue_context));
	if (!c) {
		neb_syslog(LOG_ERR, "malloc: %m");
		return NULL;
	}
	c->fd = -1;

	c->ee = malloc(q->batch_size * sizeof(struct epoll_event));
	if (!c->ee) {
		neb_syslog(LOG_ERR, "malloc: %m");
		evdp_destroy_queue_context(c);
		return NULL;
	}

	c->fd = epoll_create1(EPOLL_CLOEXEC);
	if (c->fd == -1) {
		neb_syslog(LOG_ERR, "epoll_create1: %m");
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
		struct epoll_event *e = c->ee + i;
		s_got = e->data.ptr;
		if (s_got == s_to_rm)
			e->data.ptr = NULL;
	}
}

int evdp_queue_wait_events(neb_evdp_queue_t q, struct timespec *timeout)
{
	struct evdp_queue_context *c = q->context;

	int timeout_msec;
	if (timeout) {
		timeout_msec = 0;
		if (timeout->tv_sec)
			timeout_msec += timeout->tv_sec * 1000;
		if (timeout->tv_nsec)
			timeout_msec += timeout->tv_nsec % 1000000;
	} else {
		timeout_msec = -1;
	}

	q->nevents = epoll_wait(c->fd, c->ee, q->batch_size, timeout_msec);
	if (q->nevents == -1) {
		switch (errno) {
		case EINTR:
			q->nevents = 0;
			break;
		default:
			neb_syslog(LOG_ERR, "(epoll %d)epoll_wait: %m", c->fd);
			return -1;
			break;
		}
	}
	return 0;
}

int evdp_queue_fetch_event(neb_evdp_queue_t q, struct neb_evdp_event *nee)
{
	struct evdp_queue_context *c = q->context;

	struct epoll_event *e = c->ee + q->current_event;
	nee->event = e;
	nee->source = e->data.ptr;
	return 0;
}
