
#ifndef NEB_SOCK_COMMON_H
#define NEB_SOCK_COMMON_H 1

#include <nebase/cdefs.h>

#include <sys/types.h>
#include <sys/socket.h>

/**
 * \param[in] msec timeout in milloseconds
 * \param[out] hup set if fd hup
 * \return 0 if not ready,  and errno will be set to ETIMEDOUT if timeout
 *         others if ready
 */
extern int neb_sock_timed_read_ready(int fd, int msec, int *hup)
	_nattr_warn_unused_result _nattr_nonnull((3));

/**
 * \brief check if a socket is really eof if poll_revents contains POLLIN but not POLLHUP
 * \return 0 if not eof, otherwise if eof
 */
typedef int (* neb_sock_check_eof_t)(int fd, int poll_revents, void *udata);
/**
 * \brief Function to check if the next fd event before timeout means close of peer
 * \note POLLIN means read* functions is needed to check for the real condition.
 *       it is normal that POLLHUP is not set while peer closed the connection.
 * \param[in] is_eof function to check for eof if POLLIN but not POLLHUP is set.
 *                   if NULL, just consider POLLIN as peer closed.
 * \return 1 if closed, otherwise 0, and
 *         errno will be set to ETIMEDOUT if timeout
 */
extern int neb_sock_check_peer_closed(int fd, int msec, neb_sock_check_eof_t is_eof, void *udata);

/**
 * Recv data with message boundaries, suitable for dgram and seqpacket but not stream
 * \param[in] len the exact length of the message
 * \return 0 if ok, or error
 */
extern int neb_sock_dgram_recv_exact(int fd, void *buf, size_t len)
	_nattr_nonnull((2));
/**
 * Send data with message boundaries, suitable for dgram and seqpacket but not stream
 * \param[in] len the exact length of the message
 * \return 0 if ok, or error
 */
extern int neb_sock_dgram_send_exact(int fd, const void *buf, size_t len)
	_nattr_nonnull((2));

#endif
