
#ifndef NEB_IO_H
#define NEB_IO_H 1

#include "cdefs.h"

/**
 * \param[in] fd -1 if /dev/null should be used
 */
extern int neb_io_redirect_stdin(int fd);
extern int neb_io_redirect_stdout(int fd);
extern int neb_io_redirect_stderr(int fd);
/**
 * \param[in] slave_fd should be the real slave fd
 */
extern int neb_io_redirect_pty(int slave_fd);

/**
 * \breif init terminal creation, return master side
 * \return pty master (block mode) fd or -1
 */
extern int neb_io_pty_open_master(void)
	_nattr_warn_unused_result;
/**
 * \breif finish terminal creation, and return the slave side
 * \return pty slave (block mode) fd or -1
 * \note
 *  1. chang user cred before calling this
 *  2. SIGCHLD should not be installed before call this, be care about fork(2)
 *  3. keep the returned fd open (at least for MacOS)
 */
extern int neb_io_pty_open_slave(int master_fd)
	_nattr_warn_unused_result;
/**
 * \brief associate as controlling terminal
 * \note the calling process must have no controlling terminal, use setsid(2)
 */
extern int neb_io_pty_associate(int slave_fd)
	_nattr_warn_unused_result;
/**
 * \brief disassociate as conrtolling terminal
 */
extern int neb_io_pty_disassociate(int slave_fd)
	_nattr_warn_unused_result;

#endif
