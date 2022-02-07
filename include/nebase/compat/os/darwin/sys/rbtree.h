
#ifndef NEB_COMPAT_SYS_RBTREE_H
#define NEB_COMPAT_SYS_RBTREE_H 1

#include_next <sys/rbtree.h>

#define RB_TREE_NEXT(T, N) rb_tree_iterate((T), (N), RB_DIR_RIGHT)

#endif
