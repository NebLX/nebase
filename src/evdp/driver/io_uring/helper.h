
#ifndef NEB_SRC_EVDP_DRIVER_IO_URING_HELPER_H
#define NEB_SRC_EVDP_DRIVER_IO_URING_HELPER_H 1

#include <nebase/cdefs.h>

#include "core.h"
#include "types.h"

extern int neb_io_uring_submit_fd(struct evdp_queue_context *qc, neb_evdp_source_t s)
	_nattr_warn_unused_result _nattr_nonnull((1, 2));

extern int neb_io_uring_cancel_fd(struct evdp_queue_context *qc, neb_evdp_source_t s)
	_nattr_warn_unused_result _nattr_nonnull((1, 2));;

#endif
