
#include <nebase/syslog.h>

#include "helper.h"

#include <liburing.h>

int neb_io_uring_submit_fd(struct evdp_queue_context *qc, neb_evdp_source_t s)
{
	struct evdp_source_context *sc = s->context;
	struct io_uring_sqe *sqe = io_uring_get_sqe(&qc->ring);
	if (!sqe) {
		neb_syslog(LOG_CRIT, "no sqe left");
		return -1;
	}
	io_uring_prep_poll_add(sqe, sc->fd, sc->ctl_event);
	io_uring_sqe_set_data(sqe, s);
	int ret = io_uring_submit(&qc->ring);
	if (ret < 0) {
		neb_syslogl(LOG_ERR, "io_uring_submit: %m");
		return -1;
	}
	return 0;
}

int neb_io_uring_cancel_fd(struct evdp_queue_context *qc, neb_evdp_source_t s)
{
	struct io_uring_sqe *sqe = io_uring_get_sqe(&qc->ring);
	if (!sqe) {
		neb_syslog(LOG_CRIT, "no sqe left");
		return -1;
	}
	io_uring_prep_poll_remove(sqe, s);
	int ret = io_uring_submit(&qc->ring);
	if (ret < 0) {
		neb_syslogl(LOG_ERR, "io_uring_submit: %m");
		return -1;
	}
	return 0;
}
