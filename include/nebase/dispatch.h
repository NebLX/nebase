
#ifndef NEB_DISPATCH_H
#define NEB_DISPATCH_H 1

#include "cdefs.h"

struct dispatch_queue;
typedef struct dispatch_queue* dispatch_queue_t;

struct dispatch_source;
typedef struct dispatch_source* dispatch_source_t;

/*
 * Queue Functions
 */
extern dispatch_queue_t neb_dispatch_queue_create(void)
	__attribute_warn_unused_result__;
extern void neb_dispatch_queue_destroy(dispatch_queue_t q)
	neb_attr_nonnull((1));
extern int neb_dispatch_queue_add(dispatch_queue_t q, dispatch_source_t s)
	neb_attr_nonnull((1, 2));

/*
 * Source Functions
 */
extern void neb_dispatch_source_del(dispatch_source_t s)
	neb_attr_nonnull((1));

/*
 * fd_read source
 */
typedef void (*io_handler_t)(int fd, void *udata);
extern dispatch_source_t neb_dispatch_source_new_fd_read(int fd, io_handler_t rf, void *udata)
	__attribute_warn_unused_result__ neb_attr_nonnull((2, 3));
extern dispatch_source_t neb_dispatch_source_new_fd_write(int fd, io_handler_t wf, void *udata)
	__attribute_warn_unused_result__ neb_attr_nonnull((2, 3));

/*
 * timer source
 */
#include <stdint.h>

extern dispatch_source_t neb_dispatch_source_new_itimer_sec(unsigned int ident, int64_t sec);
extern dispatch_source_t neb_dispatch_source_new_itimer_msec(unsigned int ident, int64_t msec);
extern dispatch_source_t neb_dispatch_source_new_abstimer(unsigned int ident, int sec_of_day, int interval_hour);
extern void neb_dispatch_source_abstimer_regulate(dispatch_source_t)
	neb_attr_nonnull((1));

#endif
