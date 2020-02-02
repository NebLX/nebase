
#ifndef NEB_SRC_EVDP_CORE_H
#define NEB_SRC_EVDP_CORE_H 1

#include <nebase/cdefs.h>
#include <nebase/evdp/core.h>

#include <stdint.h>
#include <stddef.h>

enum {
	EVDP_SOURCE_NONE = 0,
	EVDP_SOURCE_ITIMER_SEC,
	EVDP_SOURCE_ITIMER_MSEC,
	EVDP_SOURCE_ABSTIMER,
	EVDP_SOURCE_RO_FD,    /* read-only fd */
	EVDP_SOURCE_OS_FD,    /* oneshot fd */
	EVDP_SOURCE_LT_FD,    /* level-triggered fd */
};

#define TOTAL_DAY_SECONDS (24 * 3600)

extern void *evdp_create_queue_context(neb_evdp_queue_t q)
	_nattr_warn_unused_result _nattr_nonnull((1)) _nattr_hidden;
extern void evdp_destroy_queue_context(void *context)
	_nattr_nonnull((1)) _nattr_hidden;

struct neb_evdp_queue {
	void *context;
	int batch_size;
	int nevents;
	int current_event;

	uint32_t foreach_id; // if one source is not reached during the previous
	                     // 2^32 times of loop, we consider it meaningless to
	                     // reach it during the next loop
	uint32_t destroying:1;
	uint32_t in_foreach:1;

	neb_evdp_source_t pending_qs;
	neb_evdp_source_t running_qs;

	int64_t cur_msec;
	neb_evdp_timer_t timer;

	neb_evdp_queue_handler_t event_call;
	neb_evdp_queue_handler_t batch_call;
	void *running_udata;

	neb_evdp_source_t foreach_s;
	neb_evdp_queue_foreach_t each_call;

	struct {
		uint64_t rounds;
		uint64_t events;
		int pending;
		int running;
	} stats;
};

struct neb_evdp_source {
	neb_evdp_source_t prev;
	neb_evdp_source_t next;

	neb_evdp_queue_t q_in_use;

	int type;
	uint32_t foreach_id;
	uint32_t pending:1;   /* whether is in pending q */
	uint32_t no_detach:1; /* detach protected */

	int utype;
	void *udata;

	void *conf;
	void *context;

	neb_evdp_source_handler_t on_remove;
};

_Static_assert(sizeof(((struct neb_evdp_queue *)NULL)->foreach_id) == sizeof(((struct neb_evdp_source *)NULL)->foreach_id), "foreach_id in queue and source should match");

#define EVDP_SLIST_REMOVE(s) do { \
    if (s->prev)                  \
        s->prev->next = s->next;  \
    if (s->next)                  \
        s->next->prev = s->prev;  \
    s->prev = NULL;               \
    s->next = NULL;               \
} while(0)

#define EVDP_SLIST_INSERT_AFTER(listm, m) do { \
    if (listm->next)                           \
        listm->next->prev = m;                 \
    m->next = listm->next;                     \
    listm->next = m;                           \
    m->prev = listm;                           \
} while(0)

#define EVDP_SLIST_PENDING_INSERT(q, s) do { \
    s->next = q->pending_qs->next;           \
    s->prev = q->pending_qs;                 \
    q->pending_qs->next = s;                 \
    if (s->next)                             \
        s->next->prev = s;                   \
    s->pending = 1;                          \
    q->stats.pending++;                      \
} while(0)

#define EVDP_SLIST_RUNNING_INSERT_NO_STATS(q, s) do { \
    s->next = q->running_qs->next;                    \
    s->prev = q->running_qs;                          \
    q->running_qs->next = s;                          \
    if (s->next)                                      \
        s->next->prev = s;                            \
    s->pending = 0;                                   \
} while(0)

#define EVDP_SLIST_RUNNING_INSERT(q, s) do {  \
    EVDP_SLIST_RUNNING_INSERT_NO_STATS(q, s); \
    q->stats.running++;                       \
} while(0)

extern void evdp_queue_rm_pending_events(neb_evdp_queue_t q, neb_evdp_source_t s)
	_nattr_nonnull((1, 2)) _nattr_hidden;
/**
 * \brief waiting for events
 * \param[in] timeout NULL if should block forever
 */
extern int evdp_queue_wait_events(neb_evdp_queue_t q, int timeout_ms)
	_nattr_warn_unused_result _nattr_nonnull((1)) _nattr_hidden;

extern int evdp_queue_flush_pending_sources(neb_evdp_queue_t q)
	_nattr_warn_unused_result _nattr_nonnull((1)) _nattr_hidden;

struct neb_evdp_event {
	void *event;
	neb_evdp_source_t source;
};
/**
 * \brief fetch current event, should be in pair with finish_event
 */
extern int evdp_queue_fetch_event(neb_evdp_queue_t q, struct neb_evdp_event *nee)
	_nattr_warn_unused_result _nattr_nonnull((1, 2)) _nattr_hidden;
extern void evdp_queue_finish_event(neb_evdp_queue_t q, struct neb_evdp_event *nee)
	_nattr_nonnull((1, 2)) _nattr_hidden;

#endif
