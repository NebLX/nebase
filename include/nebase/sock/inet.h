
#ifndef NEB_SOCK_INET_H
#define NEB_SOCK_INET_H 1

#include <nebase/cdefs.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>

enum {
	NEB_CMSG_LEVEL_COMPAT = -1,
};

enum {
	NEB_CMSG_TYPE_LOOP_END = 0,   // null
	NEB_CMSG_TYPE_TIMESTAMP = 1,  // struct timespec
	NEB_CMSG_TYPE_IP4IFINDEX = 2, // unsigned int, not reliable for raw sockets
};

/**
 * \return 0 if success, others to exit the parent function with the same return value
 */
typedef int (*neb_sock_cmsg_cb)(int level, int type, const u_char *data, size_t len, void *udata);

struct neb_sock_msghdr {
	struct sockaddr  *msg_peer;
	struct iovec     *msg_iov;
	size_t            msg_iovlen;
	neb_sock_cmsg_cb  msg_control_cb;
	void             *msg_udata;
};

extern ssize_t neb_sock_inet_recvmsg(int fd, struct neb_sock_msghdr *msg)
	_nattr_warn_unused_result _nattr_nonnull((2));

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

#endif
