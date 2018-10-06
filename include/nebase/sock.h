
#ifndef NEB_SOCK_H
#define NEB_SOCK_H 1

#include <nebase/cdefs.h>

#include <sys/types.h>
#include <sys/socket.h>

/*
 * Unix Sockets
 */

#include <sys/un.h>

#define NEB_UNIX_ADDR_MAXLEN (sizeof(((struct sockaddr_un *)0)->sun_path) - 1)

/**
 * \return a new binded fd, which will be nonblock and cloexec
 *         -1 if failed
 */
extern int neb_sock_unix_new_binded(int type, const char *addr)
	__attribute_warn_unused_result__ neb_attr_nonnull((2));
/**
 * \param timeout in milliseconds
 * \return a new connected fd, which will be nonblock and cloexec
 *         -1 if failed, and errno will be set to ETIMEDOUT if timeout
 */
extern int neb_sock_unix_new_connected(int type, const char *addr, int timeout)
	__attribute_warn_unused_result__ neb_attr_nonnull((2));

struct neb_ucred {
	uid_t uid;
	gid_t gid;
	pid_t pid;
};

extern int neb_sock_unix_enable_recv_cred(int fd)
	__attribute_warn_unused_result__;
/**
 * \param[in] name Address to sendto for dgram sockets
 * \param[in] namelen Address length for dgram sockets
 */
extern int neb_sock_unix_send_with_cred(int fd, const char *data, int len, void *name, socklen_t namelen)
	__attribute_warn_unused_result__ neb_attr_nonnull((2));
/**
 * \note in nonblock mode
 */
extern int neb_sock_unix_recv_with_cred(int fd, char *data, int len, struct neb_ucred *pu)
	__attribute_warn_unused_result__ neb_attr_nonnull((2, 4));

#endif
