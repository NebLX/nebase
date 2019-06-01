
#include <nebase/syslog.h>
#include <nebase/evdp.h>
#include <nebase/time.h>

#include "core.h"

#include <stdlib.h>

static neb_evdp_source_t evdp_source_new_empty(neb_evdp_queue_t q)
{
	neb_evdp_source_t s = calloc(1, sizeof(struct neb_evdp_source));
	if (!s) {
		neb_syslog(LOG_ERR, "calloc: %m");
		return NULL;
	}
	s->type = EVDP_SOURCE_NONE;
	s->q_in_use = q;
	s->on_remove = neb_evdp_source_del;
	return s;
}

neb_evdp_queue_t neb_evdp_queue_create(int batch_size)
{
	if (batch_size <= 0)
		batch_size = NEB_EVDP_DEFAULT_BATCH_SIZE;

	neb_evdp_queue_t q = calloc(1, sizeof(struct neb_evdp_queue));
	if (!q) {
		neb_syslog(LOG_ERR, "malloc: %m");
		return NULL;
	}
	q->batch_size = batch_size;
	q->gettime = neb_time_gettime_fast;

	q->running_qs = evdp_source_new_empty(q);
	if (!q->running_qs) {
		neb_evdp_queue_destroy(q);
		return NULL;
	}
	q->running_count = 1;
	q->pending_qs = evdp_source_new_empty(q);
	if (!q->pending_qs) {
		neb_evdp_queue_destroy(q);
		return NULL;
	}
	q->pending_count = 1;

	q->context = evdp_create_queue_context(q);
	if (!q->context) {
		neb_evdp_queue_destroy(q);
		return NULL;
	}

	return q;
}

static void do_detach_from_queue(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	EVDP_SLIST_REMOVE(s);
	if (s->pending) {
		q->pending_count--;
		s->pending = 0;
	} else {
		q->running_count--;
	}

	evdp_queue_rm_pending_events(q, s);

	switch (s->type) {
	case EVDP_SOURCE_NONE:
		break;
	case EVDP_SOURCE_ITIMER_SEC:
	case EVDP_SOURCE_ITIMER_MSEC:
	case EVDP_SOURCE_ABSTIMER:
	case EVDP_SOURCE_RO_FD:
	case EVDP_SOURCE_OS_FD:
	case EVDP_SOURCE_LT_FD:
		// TODO type and platform specific detach
		break;
	default:
		neb_syslog(LOG_ERR, "Unsupported evdp_source type %d", s->type);
		break;
	}

	s->q_in_use = NULL;

	if (s->on_remove) {
		neb_evdp_source_handler_t on_remove = s->on_remove;
		int ret = on_remove(s);
		if (ret != 0)
			neb_syslog(LOG_ERR, "evdp_source %p on_remove cb %p failed with ret %d", s, on_remove, ret);
	}
}

void neb_evdp_queue_destroy(neb_evdp_queue_t q)
{
	q->destroying = 1;
	if (q->pending_count) {
		// always safe as user can not alter pending_qs within on_remove cb
		for (neb_evdp_source_t s = q->pending_qs->next; s; s = q->pending_qs->next)
			do_detach_from_queue(q, s);
		q->pending_qs->q_in_use = NULL;
		neb_evdp_source_del(q->pending_qs);
		q->pending_qs = NULL;
	}
	if (q->running_count) {
		// always safe as user can not alter running_qs within on_remove cb
		for (neb_evdp_source_t s = q->running_qs->next; s; s = q->running_qs->next)
			do_detach_from_queue(q, s);
		q->running_qs->q_in_use = NULL;
		neb_evdp_source_del(q->running_qs);
		q->running_qs = NULL;
	}

	if (q->context) {
		evdp_destroy_queue_context(q->context);
		q->context = NULL;
	}

	free(q);
}

void neb_evdp_queue_set_event_handler(neb_evdp_queue_t q, neb_evdp_queue_handler_t ef)
{
	q->event_call = ef;
}

void neb_evdp_queue_set_batch_handler(neb_evdp_queue_t q, neb_evdp_queue_handler_t bf)
{
	q->batch_call = bf;
}

void neb_evdp_queue_set_user_data(neb_evdp_queue_t q, void *udata)
{
	q->udata = udata;
}

int neb_evdp_queue_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	if (s->q_in_use) {
		neb_syslog(LOG_ERR, "It has already been added to queue %p", s->q_in_use);
		return -1;
	}
	if (q->destroying) {
		neb_syslog(LOG_ERR, "queue %p is destroying, attach is not allowed", q);
		return -1;
	}

	switch (s->type) {
	case EVDP_SOURCE_NONE:
		neb_syslog(LOG_ERR, "Empty evdp_source should not be attached");
		return -1;
		break;
	case EVDP_SOURCE_ITIMER_SEC:
	case EVDP_SOURCE_ITIMER_MSEC:
	case EVDP_SOURCE_ABSTIMER:
	case EVDP_SOURCE_RO_FD:
	case EVDP_SOURCE_OS_FD:
	case EVDP_SOURCE_LT_FD:
		// TODO type and platform specific attach (pending)
		break;
	default:
		neb_syslog(LOG_ERR, "Unsupported evdp_source type %d", s->type);
		return -1;
		break;
	}

	s->q_in_use = q;
	return 0;
}

void neb_evdp_queue_detach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	if (s->q_in_use != q) // not belong to this queue
		return;
	do_detach_from_queue(q, s);
}

static neb_evdp_cb_ret_t handle_event(neb_evdp_queue_t q)
{
	struct neb_evdp_event ne;
	if (evdp_queue_fetch_event(q, &ne) != 0)
		return NEB_EVDP_CB_BREAK;

	if (!ne.source) // source detached
		return NEB_EVDP_CB_CONTINUE;

	int ret = NEB_EVDP_CB_CONTINUE;
	switch (ne.source->type) {
	case EVDP_SOURCE_NONE:
		break;
	case EVDP_SOURCE_ITIMER_SEC:
	case EVDP_SOURCE_ITIMER_MSEC:
	case EVDP_SOURCE_ABSTIMER:
	case EVDP_SOURCE_RO_FD:
	case EVDP_SOURCE_OS_FD:
	case EVDP_SOURCE_LT_FD:
		// TODO type and platform specific handle
		break;
	default:
		neb_syslog(LOG_ERR, "Unsupported evdp_source type %d", ne.source->type);
		break;
	}

	// TODO handle return value

	return ret;
}

int neb_evdp_queue_run(neb_evdp_queue_t q)
{
	int ret = 0;

	for (;;) {
		// TODO handle events

		// TODO batch re-add

		if (q->gettime(&q->cur_ts) != 0) {
			neb_syslog(LOG_ERR, "Failed to get current time");
			ret = -1;
			goto exit_return;
		}
		struct timespec *timeout = NULL;
		// TODO get timeout

		if (evdp_queue_wait_events(q, timeout) != 0) {
			neb_syslog(LOG_ERR, "Error occured while getting evdp events");
			ret = -1;
			goto exit_return;
		}
		if (!q->nevents)
			continue;

		q->stats.rounds++;
		q->stats.events += q->nevents;

		for (int i = 0; i < q->nevents; i++) {
			q->current_event = i;
			if (handle_event(q) == NEB_EVDP_CB_BREAK)
				goto exit_return;
		}
		q->nevents = 0;
		q->current_event = 0;

		// TODO deal with timer timeout events

		if (q->batch_call && q->batch_call(q->udata) == NEB_EVDP_CB_BREAK)
			goto exit_return;
	}

exit_return:
	return ret;
}

int neb_evdp_source_del(neb_evdp_source_t s)
{
	if (s->q_in_use) {
		neb_syslog(LOG_ERR, "It is currently attached to queue %p, detach it first", s->q_in_use);
		return -1;
	}

	if (s->context) {
		switch (s->type) {
		case EVDP_SOURCE_ITIMER_SEC:
		case EVDP_SOURCE_ITIMER_MSEC:
			evdp_destroy_source_itimer_context(s->context);
			s->context = NULL;
			break;
		case EVDP_SOURCE_ABSTIMER:
		case EVDP_SOURCE_RO_FD:
		case EVDP_SOURCE_OS_FD:
		case EVDP_SOURCE_LT_FD:
			// TODO type and platform specific deinit
			break;
		default:
			neb_syslog(LOG_ERR, "Unsupported evdp_source type %d", s->type);
			break;
		}
	}

	if (s->conf)
		free(s->conf);
	free(s);
	return 0;
}

void neb_evdp_source_set_udata(neb_evdp_source_t s, void *udata)
{
	s->udata = udata;
}

void *neb_evdp_source_get_udata(neb_evdp_source_t s)
{
	return s->udata;
}

neb_evdp_queue_t neb_evdp_source_get_queue(neb_evdp_source_t s)
{
	return s->q_in_use;
}

void neb_evdp_source_set_on_remove(neb_evdp_source_t s, neb_evdp_source_handler_t on_remove)
{
	s->on_remove = on_remove;
}

neb_evdp_source_t neb_evdp_source_new_itimer_s(unsigned int ident, int val, neb_evdp_wakeup_handler_t tf)
{
	neb_evdp_source_t s = calloc(1, sizeof(struct neb_evdp_source));
	if (!s) {
		neb_syslog(LOG_ERR, "calloc: %m");
		return NULL;
	}
	s->type = EVDP_SOURCE_ITIMER_SEC;

	struct neb_evdp_conf_itimer *conf = calloc(1, sizeof(struct neb_evdp_conf_itimer));
	if (!conf) {
		neb_syslog(LOG_ERR, "calloc: %m");
		neb_evdp_source_del(s);
		return NULL;
	}
	conf->ident = ident;
	conf->sec = val;
	conf->do_wakeup = tf;
	s->conf = conf;

	s->context = evdp_create_source_itimer_context(s);
	if (!s->context) {
		neb_evdp_source_del(s);
		return NULL;
	}

	return s;
}

neb_evdp_source_t neb_evdp_source_new_itimer_ms(unsigned int ident, int val, neb_evdp_wakeup_handler_t tf)
{
	neb_evdp_source_t s = calloc(1, sizeof(struct neb_evdp_source));
	if (!s) {
		neb_syslog(LOG_ERR, "calloc: %m");
		return NULL;
	}
	s->type = EVDP_SOURCE_ITIMER_MSEC;

	struct neb_evdp_conf_itimer *conf = calloc(1, sizeof(struct neb_evdp_conf_itimer));
	if (!conf) {
		neb_syslog(LOG_ERR, "calloc: %m");
		neb_evdp_source_del(s);
		return NULL;
	}
	conf->ident = ident;
	conf->msec = val;
	conf->do_wakeup = tf;
	s->conf = conf;

	s->context = evdp_create_source_itimer_context(s);
	if (!s->context) {
		neb_evdp_source_del(s);
		return NULL;
	}

	return s;
}

neb_evdp_source_t neb_evdp_source_new_abstimer(unsigned int ident, int sec_of_day, int interval_hour, neb_evdp_wakeup_handler_t tf)
{
	neb_evdp_source_t s = calloc(1, sizeof(struct neb_evdp_source));
	if (!s) {
		neb_syslog(LOG_ERR, "calloc: %m");
		return NULL;
	}
	s->type = EVDP_SOURCE_ABSTIMER;

	struct neb_evdp_conf_abstimer *conf = calloc(1, sizeof(struct neb_evdp_conf_abstimer));
	if (!conf) {
		neb_syslog(LOG_ERR, "calloc: %m");
		neb_evdp_source_del(s);
		return NULL;
	}
	conf->ident = ident;
	conf->sec_of_day = sec_of_day;
	conf->interval_hour = interval_hour;
	conf->do_wakeup = tf;
	s->conf = conf;

	s->context = evdp_create_source_abstimer_context(s);
	if (!s->context) {
		neb_evdp_source_del(s);
		return NULL;
	}

	if (evdp_source_abstimer_regulate(s) != 0) {
		neb_syslog(LOG_ERR, "Failed to set initial wakeup time");
		return NULL;
	}

	return s;
}

int neb_evdp_source_abstimer_regulate(neb_evdp_source_t s)
{
	if (s->type != EVDP_SOURCE_ABSTIMER) {
		neb_syslog(LOG_ERR, "Invalid evdp_source type %d to regulate abstime", s->type);
		return -1;
	}
	return evdp_source_abstimer_regulate(s);
}
