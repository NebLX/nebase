
#ifndef NEB_COMPAT_SYS_QUEUE_H
#define NEB_COMPAT_SYS_QUEUE_H 1

#include_next <sys/queue.h>

#ifndef LIST_FOREACH_SAFE
# define LIST_FOREACH_SAFE(var, head, field, tvar)         \
    for ((var) = LIST_FIRST((head));                       \
         (var) && ((tvar) = LIST_NEXT((var), field), 1);   \
         (var) = (tvar))
#endif

#ifndef STAILQ_FOREACH_SAFE
# define STAILQ_FOREACH_SAFE(var, head, field, tvar)        \
    for ((var) = STAILQ_FIRST((head));                     \
         (var) && ((tvar) = STAILQ_NEXT((var), field), 1); \
         (var) = (tvar))
#endif

#endif
