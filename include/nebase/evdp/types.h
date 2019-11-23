
#ifndef NEB_EVDP_TYPES_H
#define NEB_EVDP_TYPES_H 1

struct neb_evdp_queue;
typedef struct neb_evdp_queue* neb_evdp_queue_t;

struct neb_evdp_source;
typedef struct neb_evdp_source* neb_evdp_source_t;

typedef enum {
	NEB_EVDP_CB_CONTINUE = 0,
	NEB_EVDP_CB_REMOVE,
	NEB_EVDP_CB_CLOSE,     /* remove the source as it will be closed later */
	/* NOTE, the following BREAK doesn't apply remove */
	NEB_EVDP_CB_BREAK_EXP, /* expected break */
	NEB_EVDP_CB_BREAK_ERR, /* error condition break */
	/* only for foreach cb */
	NEB_EVDP_CB_END_FOREACH,
} neb_evdp_cb_ret_t;

struct neb_evdp_timer;
typedef struct neb_evdp_timer* neb_evdp_timer_t;
typedef void * neb_evdp_timer_point;

typedef enum {
	NEB_EVDP_TIMEOUT_KEEP = 0, // keep until user call del, or timer destroyed
	NEB_EVDP_TIMEOUT_FREE = 1, // free immediatly after the callback
} neb_evdp_timeout_ret_t;

#endif
