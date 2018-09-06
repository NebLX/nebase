
#ifndef NEB_SOCK_H
#define NEB_SOCK_H 1

#include <nebase/cdefs.h>

#include <sys/types.h>
#include <sys/socket.h>

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
