
#ifndef NEB_EVDP_TIMER_H
#define NEB_EVDP_TIMER_H 1

#include <nebase/cdefs.h>
#include <nebase/evdp.h>
#include <nebase/rbtree.h>

#include <sys/queue.h>

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
	int64_t msec;
	struct evdp_timer_cblist cblist;
};

struct neb_evdp_timer {
	rb_tree_t rbtree;
	struct evdp_timer_rbtree_node *ref_min_node;
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
};

extern int evdp_timer_get_min(neb_evdp_timer_t t, int64_t cur_msec)
	_nattr_nonnull((1)) _nattr_hidden;
extern int evdp_timer_run_until(neb_evdp_timer_t t, int64_t abs_msec)
	_nattr_nonnull((1)) _nattr_hidden;

#endif
