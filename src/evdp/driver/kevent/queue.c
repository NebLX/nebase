
#include <nebase/syslog.h>

#include "core.h"
#include "types.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

void *evdp_create_queue_context(neb_evdp_queue_t q)
{
	struct evdp_queue_context *c = calloc(1, sizeof(struct evdp_queue_context));
	if (!c) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}
	c->fd = -1;

	c->ee = malloc(q->batch_size * sizeof(struct kevent));
	if (!c->ee) {
		neb_syslogl(LOG_ERR, "malloc: %m");
		evdp_destroy_queue_context(c);
		return NULL;
	}

	c->fd = kqueue();
	if (c->fd == -1) {
		neb_syslogl(LOG_ERR, "kqueue: %m");
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
		struct kevent *e = c->ee + i;
		s_got = (neb_evdp_source_t)e->udata;
		if (s_got == s_to_rm)
#if defined(OS_NETBSD)
			e->udata = (intptr_t)NULL;
#else
			e->udata = NULL;
#endif
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

	int readd_nevents = q->nevents; // re add events first
	if (readd_nevents) {
		// c->ee is reused after kevent, so we need to mv s from pending to running,
		// NOTE this may lead to pending status of s invalid, always check ENOENT
		//      when detach
		q->stats.pending -= readd_nevents;
		q->stats.running += readd_nevents;
		for (int i = 0; i < readd_nevents; i++) {
			neb_evdp_source_t s = (neb_evdp_source_t)c->ee[i].udata;
			switch (s->type) {
			case EVDP_SOURCE_OS_FD:
			{
				struct evdp_source_os_fd_context *sc = s->context;
				struct kevent *e = &c->ee[i];
				switch (e->filter) {
				case EVFILT_READ:
					sc->rd.added = 1;
					sc->rd.to_add = 0;
					break;
				case EVFILT_WRITE:
					sc->wr.added = 1;
					sc->wr.to_add = 0;
					break;
				default:
					break;
				}
				if (sc->stats_updated) {
					q->stats.pending += 1;
					q->stats.running -= 1;
					continue;
				} else {
					sc->stats_updated = 1;
				}
			}
				break;
			default:
				break;
			}
			EVDP_SLIST_REMOVE(s);
			EVDP_SLIST_RUNNING_INSERT_NO_STATS(q, s);
		}
	}
	q->nevents = kevent(c->fd, c->ee, readd_nevents, c->ee, q->batch_size, timeout);
	if (q->nevents == -1) {
		switch (errno) {
		case EINTR:
			q->nevents = 0;
			break;
		default:
			neb_syslogl(LOG_ERR, "kevent: %m");
			return -1;
			break;
		}
	}
	return 0;
}

int evdp_queue_fetch_event(neb_evdp_queue_t q, struct neb_evdp_event *nee)
{
	const struct evdp_queue_context *c = q->context;

	struct kevent *e = c->ee + q->current_event;
	nee->event = e;
	nee->source = (neb_evdp_source_t)e->udata;
	if (e->flags & EV_ERROR) { // see return value of kevent
		neb_syslogl_en(e->data, LOG_ERR, "kevent: %m");
		return -1;
	}
	return 0;
}

void evdp_queue_finish_event(neb_evdp_queue_t q _nattr_unused, struct neb_evdp_event *nee _nattr_unused)
{
	return;
}

static int do_batch_flush(neb_evdp_queue_t q, int nr)
{
	const struct evdp_queue_context *qc = q->context;
	if (kevent(qc->fd, qc->ee, nr, NULL, 0, NULL) == -1) {
		neb_syslogl(LOG_ERR, "kevent: %m");
		return -1;
	}
	q->stats.pending -= nr;
	q->stats.running += nr;
	for (int i = 0; i < nr; i++) {
		neb_evdp_source_t s = (neb_evdp_source_t)qc->ee[i].udata;
		switch (s->type) {
		case EVDP_SOURCE_OS_FD:
		{
			struct evdp_source_os_fd_context *sc = s->context;
			struct kevent *e = &qc->ee[i];
			switch (e->filter) {
			case EVFILT_READ:
				sc->rd.added = 1;
				sc->rd.to_add = 0;
				break;
			case EVFILT_WRITE:
				sc->wr.added = 1;
				sc->wr.to_add = 0;
				break;
			default:
				break;
			}
			if (sc->stats_updated) {
				q->stats.pending += 1;
				q->stats.running -= 1;
				continue;
			} else {
				sc->stats_updated = 1;
			}
		}
			break;
		default:
			break;
		}
		EVDP_SLIST_REMOVE(s);
		EVDP_SLIST_RUNNING_INSERT_NO_STATS(q, s);
	}
	return 0;
}

int evdp_queue_flush_pending_sources(neb_evdp_queue_t q)
{
	const struct evdp_queue_context *qc = q->context;
	int count = 0;
	neb_evdp_source_t last_s = NULL;
	for (neb_evdp_source_t s = q->pending_qs->next; s && last_s != s; ) {
		last_s = s;
		switch (s->type) {
		case EVDP_SOURCE_ITIMER_SEC:
		case EVDP_SOURCE_ITIMER_MSEC:
		case EVDP_SOURCE_ABSTIMER:
			memcpy(qc->ee + count++, &((struct evdp_source_timer_context *)s->context)->ctl_event, sizeof(struct kevent));
			break;
		case EVDP_SOURCE_RO_FD:
			memcpy(qc->ee + count++, &((struct evdp_source_ro_fd_context *)s->context)->ctl_event, sizeof(struct kevent));
			break;
		case EVDP_SOURCE_OS_FD: // TODO
		{
			struct evdp_source_os_fd_context *sc = s->context;
			if (sc->rd.to_add)
				memcpy(qc->ee + count++, &sc->rd.ctl_event, sizeof(struct kevent));
			if (sc->wr.to_add)
				memcpy(qc->ee + count++, &sc->wr.ctl_event, sizeof(struct kevent));
			sc->stats_updated = 0;
		}
			break;
		case EVDP_SOURCE_LT_FD: // TODO
		default:
			neb_syslog(LOG_ERR, "Unsupported pending source type %d", s->type);
			return -1;
			break;
		}
		if (count >= q->batch_size) {
			if (do_batch_flush(q, count) != 0)
				return -1;
			count = 0;
			s = q->pending_qs->next;
		} else {
			s = s->next;
		}
	}
	if (count) // the last ones will be added during wait_events
		q->nevents = count;
	return 0;
}
