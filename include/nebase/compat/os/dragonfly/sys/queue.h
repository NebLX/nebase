
#ifndef NEB_COMPAT_SYS_QUEUE_H
#define NEB_COMPAT_SYS_QUEUE_H 1

#include_next <sys/queue.h>

#ifndef LIST_FOREACH_SAFE
# define LIST_FOREACH_SAFE(var, head, field, tvar) LIST_FOREACH_MUTABLE(var, head, field, tvar)
#endif

#ifndef STAILQ_FOREACH_SAFE
# define STAILQ_FOREACH_SAFE(var, head, field, tvar) STAILQ_FOREACH_MUTABLE(var, head, field, tvar)
#endif

#endif
