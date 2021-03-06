
#ifndef NEB_SRC_EVDP_DRIVER_IO_URING_TYPES_H
#define NEB_SRC_EVDP_DRIVER_IO_URING_TYPES_H 1

#include <liburing.h>

struct evdp_queue_context {
	struct io_uring ring;
	int ring_ok;
	struct io_uring_cqe **cqe;
};

// base source context
struct evdp_source_context {
	short ctl_event; // FIXME use sqe if we need to support other types
	int fd;
	uint32_t submitted:1;
	uint32_t multishot:1;
};

struct evdp_source_timer_context {
	short ctl_event;
	int fd;
	uint32_t submitted:1;
	uint32_t multishot:1;
	int in_action;
	struct itimerspec its;
};

struct evdp_source_ro_fd_context {
	short ctl_event;
	int fd;
	uint32_t submitted:1;
	uint32_t multishot:1;
};

struct evdp_source_os_fd_context {
	short ctl_event;
	int fd;
	uint32_t submitted:1;
	uint32_t multishot:1;
	int in_callback:1;
};

#endif
