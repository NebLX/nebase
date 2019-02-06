
#ifndef NEB_DISPATCH_H
#define NEB_DISPATCH_H 1

#include "cdefs.h"

#include <stdint.h>

struct dispatch_timer;
typedef struct dispatch_timer* dispatch_timer_t;

struct dispatch_queue;
typedef struct dispatch_queue* dispatch_queue_t;

struct dispatch_source;
typedef struct dispatch_source* dispatch_source_t;

typedef enum {
	DISPATCH_CB_CONTINUE = 0,
	DISPATCH_CB_REMOVE,
	DISPATCH_CB_BREAK, // NOTE, this doesn't apply remove
	DISPATCH_CB_READD, // NOTE, for internal usage
} dispatch_cb_ret_t;

#define NEB_DISPATCH_DEFAULT_BATCH_SIZE 10

/*
 * Queue Functions
 */
typedef dispatch_cb_ret_t (*user_handler_t)(void *udata);
/**
 * \param[in] batch_size default to NEB_DISPATCH_DEFAULT_BATCH_SIZE
 */
extern dispatch_queue_t neb_dispatch_queue_create(int batch_size)
	__attribute_warn_unused_result__;
extern void neb_dispatch_queue_destroy(dispatch_queue_t q)
	neb_attr_nonnull((1));

extern void neb_dispatch_queue_set_event_handler(dispatch_queue_t q, user_handler_t ef)
	neb_attr_nonnull((1));
extern void neb_dispatch_queue_set_batch_handler(dispatch_queue_t q, user_handler_t bf)
	neb_attr_nonnull((1));
extern void neb_dispatch_queue_set_user_data(dispatch_queue_t q, void *udata)
	neb_attr_nonnull((1));

extern int neb_dispatch_queue_add(dispatch_queue_t q, dispatch_source_t s)
	__attribute_warn_unused_result__ neb_attr_nonnull((1, 2));
/**
 * \note no on_remove cb is called in this function
 */
extern void neb_dispatch_queue_rm(dispatch_queue_t q, dispatch_source_t s)
	neb_attr_nonnull((1, 2));

/**
 * \brief handler for thread events
 * \return DISPATCH_CB_BREAK if need to break, or DISPATCH_CB_CONTINUE
 */

extern int neb_dispatch_queue_run(dispatch_queue_t q)
	neb_attr_nonnull((1));

/*
 * Timer Functions
 */

/**
 * \param[in] tcache_size rbtree node cache number
 * \param[in] lcache_size cblist node cache number, should >= tcache_size
 *                        the same rbtree node may have multi cblist nodes
 */
extern dispatch_timer_t neb_dispatch_timer_create(int tcache_size, int lcache_size);
extern void neb_dispatch_timer_destroy(dispatch_timer_t t)
	neb_attr_nonnull((1));

/**
 * \brief get absolute timeout value in msec
 */
extern int64_t neb_dispatch_queue_get_abs_timeout(dispatch_queue_t q, int msec)
	neb_attr_nonnull((1));

typedef void (*timer_cb_t)(void *udata);
extern void *neb_dispatch_timer_add(dispatch_timer_t t, int64_t abs_msec, timer_cb_t cb, void *udata)
	neb_attr_nonnull((1, 3));
extern void neb_dispatch_timer_del(dispatch_timer_t t, void *n);

/*
 * Source Functions
 */

typedef void (*source_cb_t)(dispatch_source_t s);
extern int neb_dispatch_source_del(dispatch_source_t s)
	neb_attr_nonnull((1));
extern void neb_dispatch_source_set_udata(dispatch_source_t s, void *udata)
	neb_attr_nonnull((1));
extern void *neb_dispatch_source_get_udata(dispatch_source_t s)
	neb_attr_nonnull((1));
extern dispatch_queue_t neb_dispatch_source_get_queue(dispatch_source_t s)
	neb_attr_nonnull((1));
/**
 * \brief set cb that is called when ds is removed internally
 * \note this cb is not called in neb_dispatch_queue_rm
 * \note the source is not deleted after on_remove, do it yourself
 */
extern void neb_dispatch_source_set_on_remove(dispatch_source_t s, source_cb_t cb)
	neb_attr_nonnull((1, 2));
extern void neb_dispatch_source_set_readd(dispatch_source_t s, int immediatly);

/*
 * fd source
 */

/**
 * \brief io event handler
 * \return DISPATCH_CB_BREAK if error
 *         DISPATCH_CB_REMOVE if need to remove this source
 *         DISPATCH_CB_CONTINUE if continue
 */
typedef dispatch_cb_ret_t (*io_handler_t)(int fd, void *udata);
/**
 * \note hf will be called only if any of read/write callback is set
 */
extern dispatch_source_t neb_dispatch_source_new_fd(int fd, io_handler_t hf)
	__attribute_warn_unused_result__ neb_attr_nonnull((2));
/**
 * \brief an optimized fd source for read only events
 */
extern dispatch_source_t neb_dispatch_source_new_read_fd(int fd, io_handler_t rf, io_handler_t hf)
	__attribute_warn_unused_result__ neb_attr_nonnull((2, 3));
/**
 * \param[in] rf read event handler, should not be null for read_fd source
 * \param[in] wf write event handler, it will be ignored for read_fd source
 */
extern int neb_dispatch_source_fd_set_io_cb(dispatch_source_t s, io_handler_t rf, io_handler_t wf)
	neb_attr_nonnull((1));

/*
 * sys timer source
 */

/**
 * \brief timer event handler
 * \return DISPATCH_CB_BREAK if error
 *         DISPATCH_CB_REMOVE if need to remove this source
 *         DISPATCH_CB_CONTINUE if continue
 */
typedef dispatch_cb_ret_t (*timer_handler_t)(unsigned int ident, void *data);
extern dispatch_source_t neb_dispatch_source_new_itimer_sec(unsigned int ident, int64_t sec, timer_handler_t tf)
	__attribute_warn_unused_result__ neb_attr_nonnull((3));
extern dispatch_source_t neb_dispatch_source_new_itimer_msec(unsigned int ident, int64_t msec, timer_handler_t tf)
	__attribute_warn_unused_result__ neb_attr_nonnull((3));

extern dispatch_source_t neb_dispatch_source_new_abstimer(unsigned int ident, int sec_of_day, int interval_hour, timer_handler_t tf)
	__attribute_warn_unused_result__ neb_attr_nonnull((4));
extern void neb_dispatch_source_abstimer_regulate(dispatch_source_t)
	neb_attr_nonnull((1));

#endif
