
#ifndef NEB_SRC_EVDP_DRIVER_AIO_POLL_H
#define NEB_SRC_EVDP_DRIVER_AIO_POLL_H 1

#include <linux/aio_abi.h>

#include <unistd.h>
#include <poll.h> // for events
#include <sys/syscall.h>
#include <errno.h>

static inline int neb_aio_poll_create(unsigned batch_size, aio_context_t *ctx_idp)
{
	unsigned nr_events = batch_size << 3;
	*ctx_idp = 0;
	return syscall(__NR_io_setup, nr_events, ctx_idp);
}

static inline int neb_aio_poll_destroy(aio_context_t ctx_id)
{
	return syscall(__NR_io_destroy, ctx_id);
}

static inline int neb_aio_poll_submit(aio_context_t ctx_id, long nr, struct iocb **iocbpp)
{
	return syscall(__NR_io_submit, ctx_id, nr, iocbpp);
}

static inline int neb_aio_poll_cancel(aio_context_t ctx_id, struct iocb *iocb, struct io_event *result)
{
	long int ret = syscall(__NR_io_cancel, ctx_id, iocb, result);
	switch (errno) {
	case EINVAL: /* TODO remove this after io_cancel support ENOENT */
		errno = ENOENT;
		break;
	case EINPROGRESS: /* io_cancel will always return this */
		errno = 0;
		ret = 0;
		break;
	default:
		break;
	}
	return ret;
}

static inline int neb_aio_poll_wait(aio_context_t ctx_id, long nr, struct io_event *events, struct timespec *timeout)
{
	return syscall(__NR_io_getevents, ctx_id, 1, nr, events, timeout);
}

#endif
