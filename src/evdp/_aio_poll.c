
#include <nebase/syslog.h>
#include <nebase/time.h>

#include "core.h"
#include "_aio_poll.h"

#include <stdlib.h>
#include <errno.h>
#include <sys/timerfd.h>
#include <sys/socket.h>

struct evdp_queue_context {
	aio_context_t id;
	struct io_event *ee;
	struct iocb **iocbv;
};

// base source context
struct evdp_source_conext {
	struct iocb ctl_event;
	int submitted;
};

struct evdp_source_timer_context {
	struct iocb ctl_event;
	int submitted;
	int in_action;
	int fd;
	struct itimerspec its;
};

struct evdp_source_ro_fd_context {
	struct iocb ctl_event;
	int submitted;
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

	c->iocbv = malloc(q->batch_size * sizeof(struct iocb *));
	if (!c->iocbv) {
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
	const struct evdp_queue_context *c = q->context;

	struct io_event *e = c->ee + q->current_event;
	nee->event = e;
	nee->source = (neb_evdp_source_t)e->data;
	return 0;
}

static int do_batch_flush(neb_evdp_queue_t q, int nr)
{
	const struct evdp_queue_context *qc = q->context;
	if (neb_aio_poll_submit(qc->id, nr, qc->iocbv) == -1) {
		neb_syslog(LOG_ERR, "aio_poll_submit: %m");
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

void *evdp_create_source_itimer_context(neb_evdp_source_t s)
{
	struct evdp_source_timer_context *c = calloc(1, sizeof(struct evdp_source_timer_context));
	if (!c) {
		neb_syslog(LOG_ERR, "calloc: %m");
		return NULL;
	}

	struct evdp_conf_itimer *conf = s->conf;
	switch (s->type) {
	case EVDP_SOURCE_ITIMER_SEC:
		c->its.it_value.tv_sec = conf->sec;
		c->its.it_interval.tv_sec = c->its.it_value.tv_sec;
		break;
	case EVDP_SOURCE_ITIMER_MSEC:
		c->its.it_value.tv_nsec = conf->msec * 1000000;
		c->its.it_interval.tv_nsec = c->its.it_value.tv_nsec;
		break;
	default:
		neb_syslog(LOG_CRIT, "Invalid itimer source type");
		evdp_destroy_source_itimer_context(c);
		return NULL;
		break;
	}

	c->fd = timerfd_create(CLOCK_BOOTTIME, TFD_NONBLOCK | TFD_CLOEXEC);
	if (c->fd == -1) {
		neb_syslog(LOG_ERR, "timerfd_create: %m");
		evdp_destroy_source_itimer_context(c);
		return NULL;
	}
	c->submitted = 0;
	c->in_action = 0;
	s->pending = 0;

	return c;
}

void evdp_destroy_source_itimer_context(void *context)
{
	struct evdp_source_timer_context *c = context;

	if (c->fd >= 0)
		close(c->fd);
	free(c);
}

int evdp_source_itimer_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	struct evdp_source_timer_context *c = s->context;

	if (!c->in_action) {
		if (timerfd_settime(c->fd, 0, &c->its, NULL) == -1) {
			neb_syslog(LOG_ERR, "timerfd_settime: %m");
			return -1;
		}
		c->in_action = 1;
	}

	c->ctl_event.aio_lio_opcode = IOCB_CMD_POLL;
	c->ctl_event.aio_fildes = c->fd;
	c->ctl_event.aio_data = (uint64_t)s;
	c->ctl_event.aio_buf = POLLIN;

	EVDP_SLIST_PENDING_INSERT(q, s);

	return 0;
}

void evdp_source_itimer_detach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	const struct evdp_queue_context *qc = q->context;
	struct evdp_source_timer_context *sc = s->context;

	if (sc->in_action) {
		sc->its.it_value.tv_sec = 0;
		sc->its.it_value.tv_nsec = 0;
		if (timerfd_settime(sc->fd, 0, &sc->its, NULL) == -1)
			neb_syslog(LOG_ERR, "timerfd_settime: %m");
		sc->in_action = 0;
	}
	if (sc->submitted) {
		struct io_event e;
		if (neb_aio_poll_cancel(qc->id, &sc->ctl_event, &e) == -1)
			neb_syslog(LOG_ERR, "aio_poll_cancel: %m");
		sc->submitted = 0;
	}
}

neb_evdp_cb_ret_t evdp_source_itimer_handle(const struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	struct evdp_source_timer_context *sc = ne->source->context;
	sc->submitted = 0;

	const struct io_event *e = ne->event;
	const struct iocb *iocb = (struct iocb *)e->obj;

	uint64_t overrun = 0;
	if (read(iocb->aio_fildes, &overrun, sizeof(overrun)) == -1) {
		neb_syslog(LOG_ERR, "read: %m");
		return NEB_EVDP_CB_BREAK_ERR; // should not happen
	}

	const struct evdp_conf_itimer *conf = ne->source->conf;
	if (conf->do_wakeup)
		ret = conf->do_wakeup(conf->ident, overrun, ne->source->udata);
	if (ret == NEB_EVDP_CB_CONTINUE) {
		neb_evdp_queue_t q = ne->source->q_in_use;
		EVDP_SLIST_REMOVE(ne->source);
		q->stats.running--;
		EVDP_SLIST_PENDING_INSERT(ne->source->q_in_use, ne->source);
	}

	return ret;
}

void *evdp_create_source_abstimer_context(neb_evdp_source_t s)
{
	struct evdp_source_timer_context *c = calloc(1, sizeof(struct evdp_source_timer_context));
	if (!c) {
		neb_syslog(LOG_ERR, "calloc: %m");
		return NULL;
	}

	c->its.it_interval.tv_sec = TOTAL_DAY_SECONDS;
	c->its.it_interval.tv_nsec = 0;

	c->fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
	if (c->fd == -1) {
		neb_syslog(LOG_ERR, "timerfd_create: %m");
		evdp_destroy_source_itimer_context(c);
		return NULL;
	}
	c->submitted = 0;
	c->in_action = 0;
	s->pending = 0;

	return c;
}

void evdp_destroy_source_abstimer_context(void *context)
{
	struct evdp_source_timer_context *c = context;

	if (c->fd >= 0)
		close(c->fd);
	free(c);
}

int evdp_source_abstimer_regulate(neb_evdp_source_t s)
{
	struct evdp_source_timer_context *c = s->context;
	const struct evdp_conf_abstimer *conf = s->conf;

	time_t abs_ts;
	int delta_sec;
	if (neb_daytime_abs_nearest(conf->sec_of_day, &abs_ts, &delta_sec) != 0) {
		neb_syslog(LOG_ERR, "Failed to get next abs time for sec_of_day %d", conf->sec_of_day);
		return -1;
	}

	c->its.it_value.tv_sec = abs_ts;
	c->its.it_value.tv_nsec = 0;

	if (c->in_action) {
		if (timerfd_settime(c->fd, TFD_TIMER_ABSTIME, &c->its, NULL) == -1) {
			neb_syslog(LOG_ERR, "timerfd_settime: %m");
			return -1;
		}
	}

	return 0;
}

int evdp_source_abstimer_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	struct evdp_source_timer_context *c = s->context;

	if (!c->in_action) {
		if (timerfd_settime(c->fd, TFD_TIMER_ABSTIME, &c->its, NULL) == -1) {
			neb_syslog(LOG_ERR, "timerfd_settime: %m");
			return -1;
		}
		c->in_action = 1;
	}

	c->ctl_event.aio_lio_opcode = IOCB_CMD_POLL;
	c->ctl_event.aio_fildes = c->fd;
	c->ctl_event.aio_data = (uint64_t)s;
	c->ctl_event.aio_buf = POLLIN;

	EVDP_SLIST_PENDING_INSERT(q, s);

	return 0;
}

void evdp_source_abstimer_detach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	const struct evdp_queue_context *qc = q->context;
	struct evdp_source_timer_context *sc = s->context;

	if (sc->in_action) {
		sc->its.it_value.tv_sec = 0;
		sc->its.it_value.tv_nsec = 0;
		if (timerfd_settime(sc->fd, TFD_TIMER_ABSTIME, &sc->its, NULL) == -1)
			neb_syslog(LOG_ERR, "timerfd_settime: %m");
		sc->in_action = 0;
	}
	if (sc->submitted) {
		struct io_event e;
		if (neb_aio_poll_cancel(qc->id, &sc->ctl_event, &e) == -1)
			neb_syslog(LOG_ERR, "aio_poll_cancel: %m");
		sc->submitted = 0;
	}
}

neb_evdp_cb_ret_t evdp_source_abstimer_handle(const struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	struct evdp_source_timer_context *sc = ne->source->context;
	sc->submitted = 0;

	const struct io_event *e = ne->event;
	const struct iocb *iocb = (struct iocb *)e->obj;

	uint64_t overrun = 0;
	if (read(iocb->aio_fildes, &overrun, sizeof(overrun)) == -1) {
		neb_syslog(LOG_ERR, "read: %m");
		return NEB_EVDP_CB_BREAK_ERR; // should not happen
	}

	const struct evdp_conf_abstimer *conf = ne->source->conf;
	if (conf->do_wakeup)
		ret = conf->do_wakeup(conf->ident, overrun, ne->source->udata);
	if (ret == NEB_EVDP_CB_CONTINUE) {
		neb_evdp_queue_t q = ne->source->q_in_use;
		EVDP_SLIST_REMOVE(ne->source);
		q->stats.running--;
		EVDP_SLIST_PENDING_INSERT(q, ne->source);
	}

	return ret;
}

int neb_evdp_source_fd_get_sockerr(const void *context, int *sockerr)
{
	const int *fdp = context;

	socklen_t len = sizeof(int);
	if (getsockopt(*fdp, SOL_SOCKET, SO_ERROR, sockerr, &len) == -1) {
		neb_syslog(LOG_ERR, "getsockopt(SO_ERR): %m");
		return -1;
	}

	return 0;
}

void *evdp_create_source_ro_fd_context(neb_evdp_source_t s)
{
	struct evdp_source_ro_fd_context *c = calloc(1, sizeof(struct evdp_source_ro_fd_context));
	if (!c) {
		neb_syslog(LOG_ERR, "calloc: %m");
		return NULL;
	}

	c->submitted = 0;
	s->pending = 0;

	return c;
}

void evdp_destroy_source_ro_fd_context(void *context)
{
	struct evdp_source_ro_fd_context *c = context;

	free(c);
}

int evdp_source_ro_fd_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	struct evdp_source_ro_fd_context *c = s->context;
	const struct evdp_conf_ro_fd *conf = s->conf;

	c->ctl_event.aio_lio_opcode = IOCB_CMD_POLL;
	c->ctl_event.aio_fildes = conf->fd;
	c->ctl_event.aio_data = (uint64_t)s;
	c->ctl_event.aio_buf = POLLIN;

	EVDP_SLIST_PENDING_INSERT(q, s);

	return 0;
}

void evdp_source_ro_fd_detach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	const struct evdp_queue_context *qc = q->context;
	struct evdp_source_ro_fd_context *sc = s->context;

	if (sc->submitted) {
		struct io_event e;
		if (neb_aio_poll_cancel(qc->id, &sc->ctl_event, &e) == -1)
			neb_syslog(LOG_ERR, "aio_poll_cancel: %m");
		sc->submitted = 0;
	}
}

neb_evdp_cb_ret_t evdp_source_ro_fd_handle(const struct neb_evdp_event *ne)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;

	struct evdp_source_ro_fd_context *sc = ne->source->context;
	sc->submitted = 0;

	const struct io_event *e = ne->event;
	const struct iocb *iocb = (struct iocb *)e->obj;

	const int fd = iocb->aio_fildes;
	const struct evdp_conf_ro_fd *conf = ne->source->conf;
	if (e->res & POLLIN) {
		ret = conf->do_read(fd, ne->source->udata);
		if (ret != NEB_EVDP_CB_CONTINUE)
			return ret;
	}
	if (e->res & POLLHUP) {
		ret = conf->do_hup(fd, ne->source->udata, &fd);
		switch (ret) {
		case NEB_EVDP_CB_BREAK_ERR:
		case NEB_EVDP_CB_BREAK_EXP:
			break;
		default:
			ret = NEB_EVDP_CB_REMOVE;
			break;
		}
	}
	if (ret == NEB_EVDP_CB_CONTINUE) {
		neb_evdp_queue_t q = ne->source->q_in_use;
		EVDP_SLIST_REMOVE(ne->source);
		q->stats.running--;
		EVDP_SLIST_PENDING_INSERT(q, ne->source);
	}

	return ret;
}
