
#ifndef NEB_SOCK_INET_H
#define NEB_SOCK_INET_H 1

#include <nebase/cdefs.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>

extern int neb_sock_net_enable_recv_time(int fd)
	_nattr_warn_unused_result;
/**
 * \brief recv datagrams with timestamp
 * \return received datalen, or 0 if no data available, or -1 if error
 */
extern int neb_sock_net_recv_with_time(int fd, char *data, int len, struct timespec *ts)
	_nattr_warn_unused_result _nattr_nonnull((2, 4));

#endif
