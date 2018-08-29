
#ifndef NEB_SOCK_H
#define NEB_SOCK_H 1

#include <nebase/cdefs.h>

#include <sys/types.h>

struct neb_ucred {
	uid_t uid;
	gid_t gid;
	pid_t pid;
};

extern int neb_sock_unix_enable_recv_cred(int fd)
	__attribute_warn_unused_result__;
extern int neb_sock_unix_send_with_cred(int fd, const char *data, int len)
	__attribute_warn_unused_result__ neb_attr_nonnull((2));
/**
 * \note in nonblock mode
 */
extern int neb_sock_unix_recv_with_cred(int fd, char *data, int len, struct neb_ucred *pu)
	__attribute_warn_unused_result__ neb_attr_nonnull((2, 4));

#endif
