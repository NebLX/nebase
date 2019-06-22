
#ifndef NEB_EVDP_H
#define NEB_EVDP_H 1

#include "cdefs.h"

#include <stdint.h>
#include <time.h>

struct neb_evdp_timer;
typedef struct neb_evdp_timer* neb_evdp_timer_t;

struct neb_evdp_queue;
typedef struct neb_evdp_queue* neb_evdp_queue_t;

struct neb_evdp_source;
typedef struct neb_evdp_source* neb_evdp_source_t;

typedef enum {
	NEB_EVDP_CB_CONTINUE = 0,
	NEB_EVDP_CB_REMOVE,
	/* NOTE, the following BREAK doesn't apply remove */
	NEB_EVDP_CB_BREAK_EXP, /* expected break */
	NEB_EVDP_CB_BREAK_ERR, /* error condition break */
} neb_evdp_cb_ret_t;

#define NEB_EVDP_DEFAULT_BATCH_SIZE 10

/*
 * Queue Functions
 */

typedef int64_t (*neb_evdp_queue_getmsec_t)(void);
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
 * \brief get absolute timeout value in msec
 */
extern int64_t neb_evdp_queue_get_abs_timeout(neb_evdp_queue_t q, int msec)
	_nattr_nonnull((1));
/**
 * \note use this function only when really needed, i.e. timeout msec < 10
 */
extern void neb_evdp_queue_update_cur_msec(neb_evdp_queue_t q);
extern void neb_evdp_queue_set_timer(neb_evdp_queue_t q, neb_evdp_timer_t t);

extern int neb_evdp_queue_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
	_nattr_warn_unused_result _nattr_nonnull((1, 2));
extern void neb_evdp_queue_detach(neb_evdp_queue_t q, neb_evdp_source_t s)
	_nattr_nonnull((1, 2));

extern int neb_evdp_queue_run(neb_evdp_queue_t q)
	_nattr_nonnull((1));

/*
 * timer functions
 */

typedef void (*neb_evdp_timeout_handler_t)(void *udata);

/**
 * \param[in] tcache_size rbtree node cache number
 * \param[in] lcache_size cblist node cache number, should >= tcache_size
 *                        the same rbtree node may have multi cblist nodes
 */
extern neb_evdp_timer_t neb_evdp_timer_create(int tcache_size, int lcache_size);
extern void neb_evdp_timer_destroy(neb_evdp_timer_t t)
	_nattr_nonnull((1));

/**
 * \return tranparent timer, should be freed by neb_evdp_timer_del
 */
extern void *neb_evdp_timer_add(neb_evdp_timer_t t, int64_t abs_msec, neb_evdp_timeout_handler_t cb, void *udata)
	_nattr_warn_unused_result _nattr_nonnull((1, 3));
extern void neb_evdp_timer_del(neb_evdp_timer_t t, void *n)
	_nattr_nonnull((1, 2));

/*
 * Source Functions
 */

typedef int (*neb_evdp_source_handler_t)(neb_evdp_source_t s);

extern int neb_evdp_source_del(neb_evdp_source_t s)
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


/*
 * sys timer source
 */

typedef neb_evdp_cb_ret_t (*neb_evdp_wakeup_handler_t)(unsigned int ident, long overrun, void *udata);

extern neb_evdp_source_t neb_evdp_source_new_itimer_s(unsigned int ident, int val, neb_evdp_wakeup_handler_t tf)
	_nattr_warn_unused_result _nattr_nonnull((3));
extern neb_evdp_source_t neb_evdp_source_new_itimer_ms(unsigned int ident, int val, neb_evdp_wakeup_handler_t tf)
	_nattr_warn_unused_result _nattr_nonnull((3));

/**
 * \brief return a daily sys timer which will be wakeup after sec_of_day reached
 * \note Make the first wakeup callback return REMOVE, if you want a oneshot abstimer
 */
extern neb_evdp_source_t neb_evdp_source_new_abstimer(unsigned int ident, int sec_of_day, neb_evdp_wakeup_handler_t tf)
	_nattr_warn_unused_result _nattr_nonnull((3));
/**
 * \brief regulate the abstimer
 * \param[in] sec_of_day the new abstime, < 0 if you still want the old one
 */
extern int neb_evdp_source_abstimer_regulate(neb_evdp_source_t s, int sec_of_day)
	_nattr_warn_unused_result _nattr_nonnull((1));


/*
 * fd source
 */

typedef neb_evdp_cb_ret_t (*neb_evdp_eof_handler_t)(int fd, void *udata, const void *context);
typedef neb_evdp_cb_ret_t (*neb_evdp_io_handler_t)(int fd, void *udata);

extern int neb_evdp_source_fd_get_sockerr(const void *context, int *sockerr)
	_nattr_warn_unused_result _nattr_nonnull((1, 2));

extern neb_evdp_source_t neb_evdp_source_new_ro_fd(int fd, neb_evdp_io_handler_t rf, neb_evdp_eof_handler_t hf)
	_nattr_warn_unused_result _nattr_nonnull((2, 3));

#endif
