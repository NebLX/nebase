
#ifndef NH_QUERY_H
#define NH_QUERY_H 1

#include <nebase/cdefs.h>
#include <nebase/resolver.h>

#include <stdbool.h>

extern neb_resolver_t resolver;

extern void query_data_foreach_cancel(void);
extern void query_data_foreach_del(void);

extern int query_data_insert(const char *arg, int namelen, int type)
	_nattr_nonnull((1));

extern int query_data_init_submit(void);
extern int query_data_foreach_submit(int size);
extern bool query_data_foreach_submit_done(void);

#endif
