
#include <nebase/syslog.h>
#include <nebase/evdp.h>
#include <nebase/time.h>

#include "core.h"

static void remove_evdp_source(void *p)
{
	neb_evdp_source_t s = p;
	if (s->on_remove)
		s->on_remove(s);
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

	q->sources = g_hash_table_new_full(g_int64_hash, g_int64_equal, free, remove_evdp_source);
	if (!q->sources) {
		neb_syslog(LOG_ERR, "g_hash_table_new_full failed");
		neb_evdp_queue_destroy(q);
		return NULL;
	}

	q->context = evdp_create_queue_context(q);
	if (!q->context) {
		neb_evdp_queue_destroy(q);
		return NULL;
	}

	return q;
}

static void do_detach_from_queue(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	evdp_queue_rm_pending_events(q, s);

	// TODO clean re-add sources

	// TODO sys level detach

	s->q_in_use = NULL;
}

static gboolean ht_foreach_remove_source(gpointer k _nattr_unused, gpointer v, gpointer u)
{
	neb_evdp_queue_t q = u;
	neb_evdp_source_t s = v;

	if (s->q_in_use != q) // has not been attached
		return TRUE;

	do_detach_from_queue(q, s);
	return TRUE;
}

void neb_evdp_queue_destroy(neb_evdp_queue_t q)
{
	if (q->sources) {
		g_hash_table_foreach_remove(q->sources, ht_foreach_remove_source, q);
		g_hash_table_destroy(q->sources);
		q->sources = NULL;
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
	// TODO

	s->q_in_use = q;
	return 0;
}

void neb_evdp_queue_detach(neb_evdp_queue_t q, neb_evdp_source_t s)
{
	if (s->q_in_use != q) // not belong to this queue
		return;
	do_detach_from_queue(q, s);
	int64_t k = (int64_t)s;
	g_hash_table_remove(q->sources, &k);
}

static neb_evdp_cb_ret_t handle_event(neb_evdp_queue_t q)
{
	struct neb_evdp_event ne;
	if (evdp_queue_fetch_event(q, &ne) != 0)
		return NEB_EVDP_CB_BREAK;

	if (!ne.source) // source detached
		return NEB_EVDP_CB_CONTINUE;

	// TODO type specific handle

	// TODO handle return value

	return NEB_EVDP_CB_CONTINUE;
}

int neb_evdp_queue_run(neb_evdp_queue_t q)
{
	int ret = 0;

	for (;;) {
		// TODO handle events

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

		// TODO batch re-add

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
		neb_syslog(LOG_ERR, "source is currently in use, detach it first");
		return -1;
	}

	// TODO type specific deinit

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
