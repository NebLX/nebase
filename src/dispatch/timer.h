
#ifndef NEB_DISPATCH_TIMER_H
#define NEB_DISPATCH_TIMER_H 1

#include "options.h"

#include <nebase/cdefs.h>

#ifdef OS_LINUX
# ifndef __unused
#  define __unused __attribute_unused__
# endif
# include <bsd/sys/queue.h>
# include <bsd/sys/tree.h>
#else
# error "fix me"
#endif

struct dispatch_timer_cblist_node {
	LIST_ENTRY(dispatch_timer_cblist_node) node;
	timer_cb_t cb;
	void *udata;
	struct dispatch_timer_rbtree_node *ref_tnode;
};

LIST_HEAD(dispatch_timer_cblist, dispatch_timer_cblist_node);

struct dispatch_timer_rbtree_node {
	RB_ENTRY(dispatch_timer_rbtree_node) node;
	int64_t msec;
	struct dispatch_timer_cblist cblist;
};

RB_HEAD(dispatch_timer_rbtree, dispatch_timer_rbtree_node);

struct dispatch_timer {
	struct dispatch_timer_rbtree rbtree;
	struct dispatch_timer_rbtree_node *ref_min_node;
	struct {
		struct dispatch_timer_rbtree_node **nodes;
		int size;
		int count;
	} tcache;
	struct {
		struct dispatch_timer_cblist_node **nodes;
		int size;
		int count;
	} lcache;
};



#endif
