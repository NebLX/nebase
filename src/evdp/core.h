
#ifndef NEB_EVDP_CORE_H
#define NEB_EVDP_CORE_H 1

#include <nebase/cdefs.h>
#include <nebase/evdp.h>

#include <stdint.h>

enum {
	EVDP_SOURCE_NONE = 0,
	EVDP_SOURCE_ITIMER,
	EVDP_SOURCE_ABSTIMER,
	EVDP_SOURCE_RO_FD,    /* read-only fd */
	EVDP_SOURCE_OS_FD,    /* oneshot fd */
	EVDP_SOURCE_LT_FD,    /* level-triggered fd */
};

extern void *evdp_create_queue_context(neb_evdp_queue_t q)
	_nattr_warn_unused_result _nattr_nonnull((1)) _nattr_hidden;
extern void evdp_destroy_queue_context(void *context)
	_nattr_nonnull((1)) _nattr_hidden;

struct neb_evdp_queue {
	void *context;
	int batch_size;
	int nevents;
	int current_event;
	int destroying;

	int pending_count;
	int running_count;
	neb_evdp_source_t pending_qs;
	neb_evdp_source_t running_qs;

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
	neb_evdp_source_t prev;
	neb_evdp_source_t next;

	neb_evdp_queue_t q_in_use;
	int type;
	int attached; /* whether is really attached, platform specific */
	int pending;  /* whether is in pending q */

	void *context;

	neb_evdp_source_handler_t on_remove;

	void *udata;
};

#define EVDP_SLIST_REMOVE(s) do { \
    if (s->prev)                  \
        s->prev->next = s->next;  \
    if (s->next)                  \
        s->next->prev = s->prev;  \
    s->prev = NULL;               \
    s->next = NULL;               \
} while(0)

#define EVDP_SLIST_PENDING_INSERT(q, s) do { \
    s->next = q->pending_qs->next;           \
    s->prev = q->pending_qs;                 \
    q->pending_qs->next = s;                 \
    if (s->next)                             \
        s->next->prev = s;                   \
} while(0)

#define EVDP_SLIST_RUNNING_INSERT(q, s) do { \
    s->next = q->running_qs->next;           \
    s->prev = q->running_qs;                 \
    q->running_qs->next = s;                 \
    if (s->next)                             \
        s->next->prev = s;                   \
} while(0)

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
