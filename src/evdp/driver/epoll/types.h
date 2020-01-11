
#ifndef NEB_SRC_EVDP_DRIVER_EPOLL_TYPES_H
#define NEB_SRC_EVDP_DRIVER_EPOLL_TYPES_H 1

#include <sys/epoll.h>
#include <sys/timerfd.h>

struct evdp_queue_context {
	int fd;
	struct epoll_event *ee;
};

struct evdp_source_conext {
	struct epoll_event ctl_event;
	int ctl_op;
	int added;
};

struct evdp_source_timer_context {
	struct epoll_event ctl_event;
	int ctl_op;
	int added;
	int in_action;
	int fd;
	struct itimerspec its;
};

struct evdp_source_ro_fd_context {
	struct epoll_event ctl_event;
	int ctl_op;
	int added;
};

struct evdp_source_os_fd_context {
	struct epoll_event ctl_event;
	int ctl_op;
	int added;
};

#endif
