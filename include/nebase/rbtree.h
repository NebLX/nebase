
#ifndef NEB_RBTREE_H
#define NEB_RBTREE_H 1

#include <stddef.h>

#include <sys/rbtree.h>

#ifndef RB_TREE_FOREACH_SAFE
# define RB_TREE_FOREACH_SAFE(N, T, TVAR) \
	for ((N) = RB_TREE_MIN(T); (N) && ((TVAR) = rb_tree_iterate((T), (N), RB_DIR_RIGHT), 1); \
	(N) = (TVAR))
#endif

#endif
