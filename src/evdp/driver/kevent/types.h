
#ifndef NEB_SRC_EVDP_DRIVER_KEVENT_TYPES_H
#define NEB_SRC_EVDP_DRIVER_KEVENT_TYPES_H 1

#include <sys/event.h>

struct evdp_queue_context {
	int fd;
	struct kevent *ee;
};

struct evdp_source_timer_context {
	struct kevent ctl_event;
	int attached;
};

struct evdp_source_ro_fd_context {
	struct kevent ctl_event;
};

struct evdp_source_os_fd_context {
	struct {
		int added;
		int to_add;
		struct kevent ctl_event;
	} rd;
	struct {
		int added;
		int to_add;
		struct kevent ctl_event;
	} wr;
	int stats_updated;
};

#endif
