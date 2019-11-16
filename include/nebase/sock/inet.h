
#ifndef NEB_SOCK_INET_H
#define NEB_SOCK_INET_H 1

#include <nebase/cdefs.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>

/**
 * \brief get a new nonblock and cloexec socket, which can be closed by close()
 */
extern int neb_sock_inet_new(int domain, int type, int protocol)
	_nattr_warn_unused_result;

/**
 * \brief enable recv of timestamp for dgram and raw sockets
 */
extern int neb_sock_inet_enable_recv_time(int fd)
	_nattr_warn_unused_result;
/**
 * \brief recv datagrams with timestamp
 * \return received datalen, or 0 if no data available, or -1 if error
 * \note neb_sock_net_enable_recv_time is recommended to be called first,
 *       if not, neb_time_gettimeofday will be used instead.
 * \note there should be no other cmsghdr
 */
extern int neb_sock_inet_recv_with_time(int fd, struct sockaddr *addr, char *data, int len, struct timespec *ts)
	_nattr_warn_unused_result _nattr_nonnull((3, 5));

#endif
