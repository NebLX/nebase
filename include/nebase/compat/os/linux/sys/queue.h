
#ifndef NEB_COMPAT_SYS_QUEUE_H
#define NEB_COMPAT_SYS_QUEUE_H 1

#include_next <sys/queue.h>

#ifndef LIST_FOREACH_SAFE
# define LIST_FOREACH_SAFE(var, head, field, tvar)         \
    for ((var) = ((head)->lh_first);                       \
         (var) && ((tvar) = ((var)->field.le_next), 1);    \
         (var) = (tvar))
#endif

#endif
