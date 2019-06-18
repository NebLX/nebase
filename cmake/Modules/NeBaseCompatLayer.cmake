
include(CheckIncludeFile)
include(FeatureSummary)

# NetBSD sys/rbtree.h

CHECK_INCLUDE_FILE("sys/rbtree.h" HAVE_SYS_RBTREE)
add_feature_info(HAVE_SYS_RBTREE HAVE_SYS_RBTREE "sys/rbtree.h is available")
