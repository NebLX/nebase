
#ifndef NEB_EVENTS_H
#define NEB_EVENTS_H 1

enum thread_event_mask {
	T_E_NONE   = 0x0000,
	T_E_QUIT   = 0x0001,
	T_E_CHLD   = 0x0002,
};

extern _Thread_local int thread_events;

#endif
