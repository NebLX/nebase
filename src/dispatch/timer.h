
#ifndef NEB_DISPATCH_TIMER_H
#define NEB_DISPATCH_TIMER_H 1

#include "options.h"

#include <nebase/cdefs.h>

#ifdef OS_LINUX

# ifndef __unused
#  define __unused _nattr_unused
# endif
# include <bsd/sys/queue.h>

#elif defined(OS_NETBSD) || defined(OS_DARWIN)

# include <sys/queue.h>

#elif defined(OSTYPE_BSD) || defined(OS_SOLARIS)

# include <sys/queue.h>

#else
# error "fix me"
#endif

#include <stddef.h>
#include <sys/rbtree.h>

#ifndef RB_TREE_FOREACH_SAFE
# define RB_TREE_FOREACH_SAFE(N, T, TVAR) \
	for ((N) = RB_TREE_MIN(T); (N) && ((TVAR) = rb_tree_iterate((T), (N), RB_DIR_RIGHT), 1); \
	(N) = (TVAR))
#endif

struct dispatch_timer_cblist_node {
	LIST_ENTRY(dispatch_timer_cblist_node) node;
	timer_cb_t cb;
	void *udata;
	int running;
	struct dispatch_timer_rbtree_node *ref_tnode;
};

LIST_HEAD(dispatch_timer_cblist, dispatch_timer_cblist_node);

struct dispatch_timer_rbtree_node {
	rb_node_t node;
	int64_t msec;
	struct dispatch_timer_cblist cblist;
};

#if defined(HAVE_BSD_TREE)
RB_HEAD(dispatch_timer_rbtree, dispatch_timer_rbtree_node);
#endif

struct dispatch_timer {
	rb_tree_t rbtree;
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

extern int dispatch_timer_get_min(dispatch_timer_t t, int64_t cur_msec)
	_nattr_hidden _nattr_nonnull((1));
extern int dispatch_timer_run_until(dispatch_timer_t t, int64_t abs_msec)
	_nattr_hidden _nattr_nonnull((1));

#endif
