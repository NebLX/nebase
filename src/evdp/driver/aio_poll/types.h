
#ifndef NEB_SRC_EVDP_DRIVER_AIO_POLL_TYPES_H
#define NEB_SRC_EVDP_DRIVER_AIO_POLL_TYPES_H 1

#include "aio_poll.h"

#include <sys/timerfd.h>

struct evdp_queue_context {
	aio_context_t id;
	struct io_event *ee;
	struct iocb **iocbv;
};

// base source context
struct evdp_source_conext {
	struct iocb ctl_event;
	int submitted;
};

struct evdp_source_timer_context {
	struct iocb ctl_event;
	int submitted;
	int in_action;
	int fd;
	struct itimerspec its;
};

struct evdp_source_ro_fd_context {
	struct iocb ctl_event;
	int submitted;
};

struct evdp_source_os_fd_context {
	struct iocb ctl_event;
	uint32_t submitted:1;
	uint32_t in_callback:1;
};

#endif