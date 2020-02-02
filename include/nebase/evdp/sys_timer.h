
#ifndef NEB_EVDP_SYS_TIMER_H
#define NEB_EVDP_SYS_TIMER_H 1

#include <nebase/cdefs.h>

#include "types.h"

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

#endif
