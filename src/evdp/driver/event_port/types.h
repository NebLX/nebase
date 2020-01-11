
#ifndef NEB_SRC_EVDP_DRIVER_EVENT_PORT_TYPES_H
#define NEB_SRC_EVDP_DRIVER_EVENT_PORT_TYPES_H 1

#include <port.h>
#include <time.h>

struct evdp_queue_context {
	int fd;
	port_event_t *ee;
};

struct evdp_source_timer_context {
	timer_t id;
	int created;
	int in_action;
	struct itimerspec its;
};

struct evdp_source_ro_fd_context {
	int associated;
};

struct evdp_source_os_fd_context {
	int associated;
	int events;
};

#endif
