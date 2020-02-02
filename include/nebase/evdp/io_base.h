
#ifndef NEB_EVDP_IO_BASE_H
#define NEB_EVDP_IO_BASE_H 1

#include <nebase/cdefs.h>

#include "types.h"

/*
 * fd source
 *  - hf: hup handler
 *  - rf: read handler
 *  - wf: write handler
 *  hf won't be always called if peer close, read 0 in rf should be also checked,
 *  which should indicate a normal close without any error.
 */

typedef neb_evdp_cb_ret_t (*neb_evdp_io_handler_t)(int fd, void *udata, const void *context);

/**
 * \brief log sockerr and then return close
 */
extern neb_evdp_cb_ret_t neb_evdp_sock_log_on_hup(int fd, void *udata, const void *context);

/**
 * \brief get nread for the socket or pipe (Unix stream I/O)
 */
extern int neb_evdp_io_get_nread(const void *context, int *nbytes)
	_nattr_warn_unused_result _nattr_nonnull((1, 2));
/**
 * \brief get the sockerr for the socket fd
 */
extern int neb_evdp_sock_get_sockerr(const void *context, int *sockerr)
	_nattr_warn_unused_result _nattr_nonnull((1, 2));

/**
 * \param[in] rf if read return 0 in rf, it means the peer has closed with no error
 */
extern neb_evdp_source_t neb_evdp_source_new_ro_fd(int fd, neb_evdp_io_handler_t rf, neb_evdp_io_handler_t hf)
	_nattr_warn_unused_result _nattr_nonnull((2, 3));

extern neb_evdp_source_t neb_evdp_source_new_os_fd(int fd, neb_evdp_io_handler_t hf)
	_nattr_warn_unused_result _nattr_nonnull((2));

extern int neb_evdp_source_os_fd_reset(neb_evdp_source_t s, int fd)
	_nattr_warn_unused_result _nattr_nonnull((1));
/**
 * \param[in] rf set to null if you want to disable read
 *               if read return 0 in rf, it means the peer has closed with no error
 */
extern int neb_evdp_source_os_fd_next_read(neb_evdp_source_t s, neb_evdp_io_handler_t rf)
	_nattr_warn_unused_result _nattr_nonnull((1));
/**
 * \param[in] wf set to null if you want to disable write
 */
extern int neb_evdp_source_os_fd_next_write(neb_evdp_source_t s, neb_evdp_io_handler_t wf)
	_nattr_warn_unused_result _nattr_nonnull((1));

#endif
