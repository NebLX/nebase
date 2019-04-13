
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
#define NEB_UNIX_MAX_CMSG_FD 16

/**
 * \return unix sock fd, nonblock and cloexec
 */
extern int neb_sock_unix_new(int type)
	__attribute_warn_unused_result__;
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

/**
 * \param[out] in_use will be set if really in use
 * \param[out] type will be set to real type if in use, and 0 if not
 * \return  0 if check ok, values will be set in out params
 *         -1 if check failed
 *          1 if check is not suported
 */
extern int neb_sock_unix_path_in_use(const char *path, int *in_use, int *type)
	neb_attr_nonnull((1, 2, 3));

/**
 * \brief cmsg size to be used with unix ucred
 */
extern size_t neb_sock_ucred_cmsg_size;

struct neb_ucred {
	uid_t uid;
	gid_t gid;
	pid_t pid;
};

/**
 * \note this function should be called before accept *and* poll/recv,
 *       to be compitable with different OSs
 */
extern int neb_sock_unix_enable_recv_cred(int fd)
	__attribute_warn_unused_result__;
/**
 * \param[in] name Address to sendto for dgram sockets
 * \param[in] namelen Address length for dgram sockets
 */
extern int neb_sock_unix_send_with_cred(int fd, const char *data, int len, void *name, socklen_t namelen)
	__attribute_warn_unused_result__ neb_attr_nonnull((2));
/**
 * \note in nonblock mode, the caller should make sure there is data available
 */
extern int neb_sock_unix_recv_with_cred(int fd, char *data, int len, struct neb_ucred *pu)
	__attribute_warn_unused_result__ neb_attr_nonnull((2, 4));

/**
 * \param[in] fds must not be NULL, contains fd_num of fds
 * \param[in] fd_num must not be 0, and must < NEB_UNIX_MAX_CMSG_FD
 */
extern int neb_sock_unix_send_with_fds(int fd, const char *data, int len, int *fds, int fd_num, void *name, socklen_t namelen)
	__attribute_warn_unused_result__ neb_attr_nonnull((2, 4));
/**
 * \param[in] fds array to store received fd, should be *fd_num elements
 * \param[in,out] fd_num may be 0 after return, and must < NEB_UNIX_MAX_CMSG_FD
 * \note close fds if fd_num is not zero
 * \note the fds returned will be cloexec
 */
extern int neb_sock_unix_recv_with_fds(int fd, char *data, int len, int *fds, int *fd_num)
	__attribute_warn_unused_result__ neb_attr_nonnull((2, 4));

/*
 * Common Functions
 */

/**
 * \param[in] msec timeout in milloseconds
 * \param[out] hup set if fd hup
 * \return 0 if not ready, otherwise 1, errno will be set to indicate real error
 */
extern int neb_sock_timed_read_ready(int fd, int msec, int *hup)
	__attribute_warn_unused_result__ neb_attr_nonnull((3));

/**
 * Recv data with message boundaries, suitable for dgram and seqpacket but not stream
 * \param[in] len the exact length of the message
 * \return 0 if ok, or error
 */
extern int neb_sock_recv_exact(int fd, void *buf, size_t len)
	neb_attr_nonnull((2));
/**
 * Send data with message boundaries, suitable for dgram and seqpacket but not stream
 * \param[in] len the exact length of the message
 * \return 0 if ok, or error
 */
extern int neb_sock_send_exact(int fd, const void *buf, size_t len)
	neb_attr_nonnull((2));

#endif
