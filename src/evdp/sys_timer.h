
#ifndef NEB_SRC_EVDP_SYS_TIMER_H
#define NEB_SRC_EVDP_SYS_TIMER_H 1

#include <nebase/evdp/sys_timer.h>

struct evdp_conf_itimer {
	unsigned int ident;
	union {
		int64_t sec;
		int64_t msec;
	};
	neb_evdp_wakeup_handler_t do_wakeup;
};
extern void *evdp_create_source_itimer_context(neb_evdp_source_t s)
	_nattr_warn_unused_result _nattr_nonnull((1)) _nattr_hidden;
extern void evdp_destroy_source_itimer_context(void *context)
	_nattr_nonnull((1)) _nattr_hidden;

struct evdp_conf_abstimer {
	unsigned int ident;
	int sec_of_day;
	neb_evdp_wakeup_handler_t do_wakeup;
};
extern void *evdp_create_source_abstimer_context(neb_evdp_source_t s)
	_nattr_warn_unused_result _nattr_nonnull((1)) _nattr_hidden;
extern void evdp_destroy_source_abstimer_context(void *context)
	_nattr_nonnull((1)) _nattr_hidden;
extern int evdp_source_abstimer_regulate(neb_evdp_source_t s)
	_nattr_warn_unused_result _nattr_nonnull((1)) _nattr_hidden;

extern int evdp_source_itimer_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
	_nattr_warn_unused_result _nattr_nonnull((1, 2)) _nattr_hidden;
extern void evdp_source_itimer_detach(neb_evdp_queue_t q, neb_evdp_source_t s)
	_nattr_nonnull((1, 2)) _nattr_hidden;
extern neb_evdp_cb_ret_t evdp_source_itimer_handle(const struct neb_evdp_event *ne)
	_nattr_warn_unused_result _nattr_nonnull((1)) _nattr_hidden;

extern int evdp_source_abstimer_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
	_nattr_warn_unused_result _nattr_nonnull((1, 2)) _nattr_hidden;
extern void evdp_source_abstimer_detach(neb_evdp_queue_t q, neb_evdp_source_t s)
	_nattr_nonnull((1, 2)) _nattr_hidden;
extern neb_evdp_cb_ret_t evdp_source_abstimer_handle(const struct neb_evdp_event *ne)
	_nattr_warn_unused_result _nattr_nonnull((1)) _nattr_hidden;

#endif
