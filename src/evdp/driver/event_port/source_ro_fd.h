
#ifndef NEB_SRC_EVDP_DRIVER_EVENT_POLL_SOURCE_RO_FD_H
#define NEB_SRC_EVDP_DRIVER_EVENT_POLL_SOURCE_RO_FD_H 1

#include <nebase/cdefs.h>
#include <nebase/evdp/core.h>

#include "types.h"

extern int do_associate_ro_fd(const struct evdp_queue_context *qc, neb_evdp_source_t s)
	_nattr_warn_unused_result _nattr_nonnull((1, 2)) _nattr_hidden;

#endif
