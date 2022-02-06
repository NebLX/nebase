
#ifndef NEB_SRC_EVDP_TIMER_H
#define NEB_SRC_EVDP_TIMER_H 1

#include <nebase/cdefs.h>
#include <nebase/evdp/core.h>
#include <nebase/rbtree.h>

#include <sys/queue.h>
#include <time.h>

struct evdp_timer_cblist_node {
	LIST_ENTRY(evdp_timer_cblist_node) list;
	neb_evdp_timeout_handler_t on_timeout;
	void *udata;
	int running;
	struct evdp_timer_rbtree_node *ref_tnode;
};

LIST_HEAD(evdp_timer_cblist, evdp_timer_cblist_node);

struct evdp_timer_rbtree_node {
	rb_node_t rbtree_context;
	struct timespec ts;
	struct evdp_timer_cblist cblist;
	int no_auto_del;
};

struct neb_evdp_timer {
	rb_tree_t rbtree;
	struct {
		struct evdp_timer_rbtree_node **nodes;
		int size;
		int count;
	} tcache;
	struct {
		struct evdp_timer_cblist_node **nodes;
		int size;
		int count;
	} lcache;
	struct evdp_timer_cblist keeplist;
};

extern struct timespec *evdp_timer_fetch_neareast_ts(neb_evdp_timer_t t, struct timespec *cur_ts)
	_nattr_nonnull((1, 2)) _nattr_warn_unused_result _nattr_hidden;
extern int evdp_timer_run_until(neb_evdp_timer_t t, struct timespec *abs_ts)
	_nattr_nonnull((1)) _nattr_hidden;

#endif
