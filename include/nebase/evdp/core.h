
#ifndef NEB_EVDP_CORE_H
#define NEB_EVDP_CORE_H 1

#include <nebase/cdefs.h>
#include <stdint.h>
#include <time.h>

#include "types.h"

#define NEB_EVDP_DEFAULT_BATCH_SIZE 10

/*
 * Queue Functions
 */

typedef neb_evdp_cb_ret_t (*neb_evdp_queue_handler_t)(void *udata);

/**
 * \param[in] batch_size default to NEB_EVDP_DEFAULT_BATCH_SIZE
 */
extern neb_evdp_queue_t neb_evdp_queue_create(int batch_size)
	_nattr_warn_unused_result;
extern void neb_evdp_queue_destroy(neb_evdp_queue_t q)
 	_nattr_nonnull((1));

/**
 * \param[in] ef if NULL, the default one will be used
 */
extern void neb_evdp_queue_set_event_handler(neb_evdp_queue_t q, neb_evdp_queue_handler_t ef)
	_nattr_nonnull((1));
/**
 * \param[in] bf NULL if there should be no batch action
 */
extern void neb_evdp_queue_set_batch_handler(neb_evdp_queue_t q, neb_evdp_queue_handler_t bf)
	_nattr_nonnull((1));
extern void neb_evdp_queue_set_user_data(neb_evdp_queue_t q, void *udata)
	_nattr_nonnull((1));

/**
 * \brief get absolute timeout value
 */
extern void neb_evdp_queue_get_abs_timeout(neb_evdp_queue_t q, struct timespec *dur_ts, struct timespec *abs_ts)
	_nattr_nonnull((1, 2, 3));
static inline void neb_evdp_queue_get_abs_timeout_s(neb_evdp_queue_t q, int sec, struct timespec *abs_ts)
{
	struct timespec dur_ts = { .tv_sec = sec, .tv_nsec = 0 };
	neb_evdp_queue_get_abs_timeout(q, &dur_ts, abs_ts);
}
static inline void neb_evdp_queue_get_abs_timeout_ms(neb_evdp_queue_t q, int msec, struct timespec *abs_ts)
{
	struct timespec dur_ts = { .tv_sec = 0, .tv_nsec = msec * 1000000 };
	neb_evdp_queue_get_abs_timeout(q, &dur_ts, abs_ts);
}
/**
 * \note use this function only when really needed, i.e. timeout msec < 10
 */
extern void neb_evdp_queue_update_cur_ts(neb_evdp_queue_t q)
	_nattr_nonnull((1));
extern void neb_evdp_queue_set_timer(neb_evdp_queue_t q, neb_evdp_timer_t t)
	_nattr_nonnull((1, 2));
extern neb_evdp_timer_t neb_evdp_queue_get_timer(neb_evdp_queue_t q)
	_nattr_nonnull((1));

extern int neb_evdp_queue_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
	_nattr_warn_unused_result _nattr_nonnull((1, 2));
/**
 * \param[in] to_close set if the underlying fd is closed or going to close before next wait_events
 */
extern int neb_evdp_queue_detach(neb_evdp_queue_t q, neb_evdp_source_t s, int to_close)
	_nattr_warn_unused_result _nattr_nonnull((1, 2));

extern int neb_evdp_queue_run(neb_evdp_queue_t q)
	_nattr_nonnull((1));

/**
 * \return NEB_EVDP_CB_CONTINUE or NEB_EVDP_CB_REMOVE or NEB_EVDP_CB_END_FOREACH
 * \note for NEB_EVDP_CB_REMOVE, there may be a later batch remove after all sources checked
 */
typedef neb_evdp_cb_ret_t (*neb_evdp_queue_foreach_t)(neb_evdp_source_t s, int utype, void *udata);
/**
 * \breif call cb on each running sources
 * \return current running source count, or -1 if error
 */
extern int neb_evdp_queue_foreach_start(neb_evdp_queue_t q, neb_evdp_queue_foreach_t cb)
	_nattr_warn_unused_result _nattr_nonnull((1));
/**
 * \return handled running source count, or -1 if error
 * \note sources that inserted between foreach_next calls will not be checked again,
 *       and sources without utype set will be skipped
 */
extern int neb_evdp_queue_foreach_next(neb_evdp_queue_t q, int batch_size);
extern int neb_evdp_queue_foreach_has_ended(neb_evdp_queue_t q)
	_nattr_nonnull((1));
/**
 * \note must be called to end foreach_start
 */
extern void neb_evdp_queue_foreach_set_end(neb_evdp_queue_t q)
	_nattr_nonnull((1));

/*
 * timer functions
 */

/**
 * \return NEB_EVDP_TIMEOUT_FREE to auto free after this function
 *         NEB_EVDP_TIMEOUT_KEEP to do nothing
 */
typedef neb_evdp_timeout_ret_t (*neb_evdp_timeout_handler_t)(void *udata);

/**
 * \param[in] tcache_size rbtree node cache number
 * \param[in] lcache_size cblist node cache number, should >= tcache_size
 *                        the same rbtree node may have multi cblist nodes
 */
extern neb_evdp_timer_t neb_evdp_timer_create(int tcache_size, int lcache_size);
/**
 * \brief destroy the timer, all pending and kept points will also be freed
 */
extern void neb_evdp_timer_destroy(neb_evdp_timer_t t)
	_nattr_nonnull((1));

/**
 * \return tranparent timer, should be freed by neb_evdp_timer_del
 */
extern neb_evdp_timer_point neb_evdp_timer_new_point(neb_evdp_timer_t t, struct timespec *abs_ts, neb_evdp_timeout_handler_t cb, void *udata)
	_nattr_warn_unused_result _nattr_nonnull((1, 2, 3));
extern void neb_evdp_timer_del_point(neb_evdp_timer_t t, neb_evdp_timer_point p)
	_nattr_nonnull((1, 2));
extern int neb_evdp_timer_point_reset(neb_evdp_timer_t t, neb_evdp_timer_point p, struct timespec *abs_ts)
	_nattr_warn_unused_result _nattr_nonnull((1, 2, 3));

/*
 * Source Functions
 */

typedef int (*neb_evdp_source_handler_t)(neb_evdp_source_t s);

extern int neb_evdp_source_del(neb_evdp_source_t s)
	_nattr_nonnull((1));
/**
 * \brief set utype for foreach call
 * \param[in] utype a user set value for use in foreach callback.
 *  it should be set to a non-zero value if you want it to be called
 */
extern void neb_evdp_source_set_utype(neb_evdp_source_t s, int utype)
	_nattr_nonnull((1));
extern void neb_evdp_source_set_udata(neb_evdp_source_t s, void *udata)
	_nattr_nonnull((1));
extern void *neb_evdp_source_get_udata(neb_evdp_source_t s)
	_nattr_nonnull((1));
extern neb_evdp_queue_t neb_evdp_source_get_queue(neb_evdp_source_t s)
	_nattr_nonnull((1));
/**
 * \brief set cb that is called when source is removed from queue
 * \note the source is not auto deleted after on_remove, but you can always call
 *       it before exiting on_remove
 */
extern void neb_evdp_source_set_on_remove(neb_evdp_source_t s, neb_evdp_source_handler_t on_remove)
	_nattr_nonnull((1, 2));

#endif
