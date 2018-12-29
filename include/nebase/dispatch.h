
#ifndef NEB_DISPATCH_H
#define NEB_DISPATCH_H 1

#include "cdefs.h"

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
typedef dispatch_cb_ret_t (*batch_handler_t)(void *udata);
/**
 * \param[in] batch_size default to NEB_DISPATCH_DEFAULT_BATCH_SIZE
 */
extern dispatch_queue_t neb_dispatch_queue_create(batch_handler_t bf, int batch_size, void *udata)
	__attribute_warn_unused_result__;
extern void neb_dispatch_queue_destroy(dispatch_queue_t q)
	neb_attr_nonnull((1));
extern int neb_dispatch_queue_add(dispatch_queue_t q, dispatch_source_t s)
	__attribute_warn_unused_result__ neb_attr_nonnull((1, 2));
/**
 * \note no on_remove cb is called in this function
 */
extern int neb_dispatch_queue_rm(dispatch_queue_t q, dispatch_source_t s)
	__attribute_warn_unused_result__ neb_attr_nonnull((1, 2));

/**
 * \brief handler for thread events
 * \return DISPATCH_CB_BREAK if need to break, or DISPATCH_CB_CONTINUE
 */
typedef dispatch_cb_ret_t (*tevent_handler_t)(void *udata);
extern int neb_dispatch_queue_run(dispatch_queue_t q, tevent_handler_t tef, void *udata)
	neb_attr_nonnull((1));

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
extern dispatch_source_t neb_dispatch_source_new_fd_read(int fd, io_handler_t rf, io_handler_t hf)
	__attribute_warn_unused_result__ neb_attr_nonnull((2, 3));
extern dispatch_source_t neb_dispatch_source_new_fd_write(int fd, io_handler_t wf, io_handler_t hf)
	__attribute_warn_unused_result__ neb_attr_nonnull((2, 3));

/*
 * timer source
 */

#include <stdint.h>

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
