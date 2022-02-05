
#include <nebase/syslog.h>

#include "core.h"
#include "io_base.h"
#include "types.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <linux/version.h>

#if __GLIBC_PREREQ(2, 35)
# define USE_EPOLL_WAIT2
static inline int epoll_wait2(int epfd, struct epoll_event *events,
                              int maxevents, const struct timespec *timeout)
{
	return epoll_pwait2(epfd, events, maxevents, timeout, NULL);
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
# define USE_EPOLL_WAIT2

# include <sys/syscall.h>
# include <linux/time_types.h>

static inline int epoll_wait2(int epfd, struct epoll_event *events,
                              int maxevents, const struct timespec *timeout)
{
	struct __kernel_timespec ts;
	struct __kernel_timespec *timeout_k = NULL;
	if (timeout != NULL) {
		ts.tv_sec = timeout->tv_sec;
		ts.tv_nsec = timeout->tv_nsec;
		timeout_k = &ts;
	}
	return syscall(__NR_epoll_pwait2, epfd, events, maxevents, timeout_k, NULL, sizeof(sigset_t));
}
#endif

void *evdp_create_queue_context(neb_evdp_queue_t q)
{
	struct evdp_queue_context *c = calloc(1, sizeof(struct evdp_queue_context));
	if (!c) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}
	c->fd = -1;

	c->ee = malloc(q->batch_size * sizeof(struct epoll_event));
	if (!c->ee) {
		neb_syslogl(LOG_ERR, "malloc: %m");
		evdp_destroy_queue_context(c);
		return NULL;
	}

	c->fd = epoll_create1(EPOLL_CLOEXEC);
	if (c->fd == -1) {
		neb_syslogl(LOG_ERR, "epoll_create1: %m");
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
		struct epoll_event *e = c->ee + i;
		s_got = e->data.ptr;
		if (s_got == s_to_rm)
			e->data.ptr = NULL;
	}
}

int evdp_queue_wait_events(neb_evdp_queue_t q, struct timespec *timeout)
{
	const struct evdp_queue_context *c = q->context;

#ifdef USE_EPOLL_WAIT2
	q->nevents = epoll_wait2(c->fd, c->ee, q->batch_size, timeout);
#else
	int timeout_msec = timeout->tv_sec * 1000 + timeout->tv_nsec / 1000000;
	q->nevents = epoll_wait(c->fd, c->ee, q->batch_size, timeout_msec);
#endif
	if (q->nevents == -1) {
		switch (errno) {
		case EINTR:
			q->nevents = 0;
			break;
		default:
			neb_syslogl(LOG_ERR, "epoll_wait: %m");
			return -1;
			break;
		}
	}
	return 0;
}

int evdp_queue_fetch_event(neb_evdp_queue_t q, struct neb_evdp_event *nee)
{
	const struct evdp_queue_context *c = q->context;

	struct epoll_event *e = c->ee + q->current_event;
	nee->event = e;
	nee->source = e->data.ptr;
	return 0;
}

void evdp_queue_finish_event(neb_evdp_queue_t q _nattr_unused, struct neb_evdp_event *nee _nattr_unused)
{
	return;
}

int evdp_queue_flush_pending_sources(neb_evdp_queue_t q)
{
	struct evdp_queue_context *qc = q->context;
	int count = 0;
	for (neb_evdp_source_t s = q->pending_qs->next; s; s = q->pending_qs->next) {
		struct evdp_source_conext *sc = s->context;
		int fd;
		switch (s->type) {
		case EVDP_SOURCE_ITIMER_SEC:
		case EVDP_SOURCE_ITIMER_MSEC:
		case EVDP_SOURCE_ABSTIMER:
			fd = ((struct evdp_source_timer_context *)s->context)->fd;
			break;
		case EVDP_SOURCE_RO_FD:
			fd = ((struct evdp_conf_ro_fd *)s->conf)->fd;
			break;
		case EVDP_SOURCE_OS_FD:
			fd = ((struct evdp_conf_fd *)s->conf)->fd;
			break;
		case EVDP_SOURCE_LT_FD: // TODO
		default:
			neb_syslog(LOG_ERR, "Unsupported epoll(ADD/MOD) source type %d", s->type);
			return -1;
			break;
		}
		if (epoll_ctl(qc->fd, sc->ctl_op, fd, &sc->ctl_event) == -1) {
			neb_syslogl(LOG_ERR, "epoll_ctl(op:%d): %m", sc->ctl_op);
			return -1;
		}
		sc->added = 1;
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
