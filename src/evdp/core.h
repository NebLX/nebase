
#ifndef NEB_EVDP_CORE_H
#define NEB_EVDP_CORE_H 1

#include <nebase/cdefs.h>
#include <nebase/evdp.h>

#include <stdint.h>

#include <glib.h>

extern void *evdp_create_queue_context(neb_evdp_queue_t q)
	_nattr_warn_unused_result _nattr_nonnull((1)) _nattr_hidden;
extern void evdp_destroy_queue_context(void *context)
	_nattr_nonnull((1)) _nattr_hidden;

struct neb_evdp_queue {
	void *context;
	int batch_size;
	int nevents;
	int current_event;

	GHashTable *sources;

	struct timespec cur_ts;
	neb_evdp_queue_gettime_t gettime;

	neb_evdp_queue_handler_t event_call;
	neb_evdp_queue_handler_t batch_call;

	void *udata;

	struct {
		uint64_t rounds;
		uint64_t events;
	} stats;
};

struct neb_evdp_source {
	int type;
	neb_evdp_queue_t q_in_use;



	neb_evdp_source_handler_t on_remove;

	void *udata;
};

extern void evdp_queue_rm_pending_events(neb_evdp_queue_t q, neb_evdp_source_t s)
	_nattr_nonnull((1, 2)) _nattr_hidden;
/**
 * \brief waiting for events
 * \param[in] timeout NULL if should block forever
 */
extern int evdp_queue_wait_events(neb_evdp_queue_t q, struct timespec *timeout)
	_nattr_warn_unused_result _nattr_nonnull((1)) _nattr_hidden;

struct neb_evdp_event {
	void *event;
	neb_evdp_source_t source;
};
/**
 * \brief fetch current event
 */
extern int evdp_queue_fetch_event(neb_evdp_queue_t q, struct neb_evdp_event *nee)
	_nattr_warn_unused_result _nattr_nonnull((1, 2)) _nattr_hidden;

#endif
