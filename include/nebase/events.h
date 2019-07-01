
#ifndef NEB_EVENTS_H
#define NEB_EVENTS_H 1

enum thread_event_mask {
	T_E_NONE   = 0x00000000,
	T_E_MORE   = 0x00000001, /* There are more detailed events to check */
	T_E_QUIT   = 0x00000002, /* General process/thread quit event */
	T_E_CHLD   = 0x00000004,
	T_E_EVDP   = 0x00000008, /* evdp specific events, i.e. evdp foreach */
};

#include <signal.h>

extern _Thread_local volatile sig_atomic_t thread_events;

#endif
