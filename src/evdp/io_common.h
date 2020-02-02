
#ifndef NEB_SRC_EVDP_IO_COMMON_H
#define NEB_SRC_EVDP_IO_COMMON_H 1

#include <nebase/evdp/io_common.h>

struct evdp_conf_ro_fd {
	int fd;
	neb_evdp_io_handler_t do_hup;
	neb_evdp_io_handler_t do_read;
};
extern void *evdp_create_source_ro_fd_context(neb_evdp_source_t s)
	_nattr_warn_unused_result _nattr_nonnull((1)) _nattr_hidden;
extern void evdp_destroy_source_ro_fd_context(void *context)
	_nattr_nonnull((1)) _nattr_hidden;

struct evdp_conf_fd {
	int fd;
	neb_evdp_io_handler_t do_hup;
	neb_evdp_io_handler_t do_read;
	neb_evdp_io_handler_t do_write;
};
extern void *evdp_create_source_os_fd_context(neb_evdp_source_t s)
	_nattr_warn_unused_result _nattr_nonnull((1)) _nattr_hidden;
extern void evdp_reset_source_os_fd_context(neb_evdp_source_t s)
	_nattr_nonnull((1)) _nattr_hidden;
extern void evdp_destroy_source_os_fd_context(void *context)
	_nattr_nonnull((1)) _nattr_hidden;

extern int evdp_source_ro_fd_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
	_nattr_warn_unused_result _nattr_nonnull((1, 2)) _nattr_hidden;
extern void evdp_source_ro_fd_detach(neb_evdp_queue_t q, neb_evdp_source_t s, int to_close)
	_nattr_nonnull((1, 2)) _nattr_hidden;
extern neb_evdp_cb_ret_t evdp_source_ro_fd_handle(const struct neb_evdp_event *ne)
	_nattr_warn_unused_result _nattr_nonnull((1)) _nattr_hidden;

extern int evdp_source_os_fd_attach(neb_evdp_queue_t q, neb_evdp_source_t s)
	_nattr_warn_unused_result _nattr_nonnull((1, 2)) _nattr_hidden;
extern void evdp_source_os_fd_detach(neb_evdp_queue_t q, neb_evdp_source_t s, int to_close)
	_nattr_nonnull((1, 2)) _nattr_hidden;
extern neb_evdp_cb_ret_t evdp_source_os_fd_handle(const struct neb_evdp_event *ne)
	_nattr_warn_unused_result _nattr_nonnull((1)) _nattr_hidden;
extern void evdp_source_os_fd_init_read(neb_evdp_source_t s, neb_evdp_io_handler_t rf)
	_nattr_nonnull((1)) _nattr_hidden;
extern int evdp_source_os_fd_reset_read(neb_evdp_source_t s)
	_nattr_warn_unused_result _nattr_nonnull((1)) _nattr_hidden;
extern int evdp_source_os_fd_unset_read(neb_evdp_source_t s)
	_nattr_warn_unused_result _nattr_nonnull((1)) _nattr_hidden;
extern void evdp_source_os_fd_init_write(neb_evdp_source_t s, neb_evdp_io_handler_t rf)
	_nattr_nonnull((1)) _nattr_hidden;
extern int evdp_source_os_fd_reset_write(neb_evdp_source_t s)
	_nattr_warn_unused_result _nattr_nonnull((1)) _nattr_hidden;
extern int evdp_source_os_fd_unset_write(neb_evdp_source_t s)
	_nattr_warn_unused_result _nattr_nonnull((1)) _nattr_hidden;

#endif
