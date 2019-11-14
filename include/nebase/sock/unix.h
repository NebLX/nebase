
#ifndef NEB_SOCK_UNIX_H
#define NEB_SOCK_UNIX_H 1

#include <nebase/cdefs.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define NEB_UNIX_ADDR_MAXLEN (sizeof(((struct sockaddr_un *)0)->sun_path) - 1)
#define NEB_UNIX_MAX_CMSG_FD 16

/**
 * \return unix sock fd, nonblock and cloexec
 */
extern int neb_sock_unix_new(int type)
	_nattr_warn_unused_result;
/**
 * \return a new binded fd, which will be nonblock and cloexec
 *         -1 if failed
 */
extern int neb_sock_unix_new_binded(int type, const char *addr)
	_nattr_warn_unused_result _nattr_nonnull((2));
/**
 * \param timeout in milliseconds
 * \return a new connected fd, which will be nonblock and cloexec
 *         -1 if failed, and errno will be set to ETIMEDOUT if timeout
 */
extern int neb_sock_unix_new_connected(int type, const char *addr, int timeout)
	_nattr_warn_unused_result _nattr_nonnull((2));

/**
 * \param[out] in_use will be set if really in use
 * \param[out] type will be set to real type if in use, and 0 if not
 * \return  0 if check ok, values will be set in out params
 *         -1 if check failed
 *          1 if check is not suported
 */
extern int neb_sock_unix_path_in_use(const char *path, int *in_use, int *type)
	_nattr_nonnull((1, 2, 3));

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
extern int neb_sock_unix_enable_recv_cred(int type, int fd)
	_nattr_warn_unused_result;
extern int neb_sock_unix_disable_recv_cred(int type, int fd)
	_nattr_warn_unused_result;
/**
 * \param[in] name Address to sendto for dgram sockets, should be NULL for non-dgram sockets
 * \param[in] namelen Address length for dgram sockets
 */
extern int neb_sock_unix_send_with_cred(int fd, const char *data, int len, void *name, socklen_t namelen)
	_nattr_warn_unused_result _nattr_nonnull((2));
/**
 * \breif fetch user cred with the first read of data
 * \return received datalen, or 0 if no data available, or -1 if error
 * \note this function will disable the next recv of cred if sock options is used internally
 */
extern int neb_sock_unix_recv_with_cred(int type, int fd, char *data, int len, struct neb_ucred *pu)
	_nattr_warn_unused_result _nattr_nonnull((3, 5));

/**
 * \param[in] fds must not be NULL, contains fd_num of fds
 * \param[in] fd_num must not be 0, and must < NEB_UNIX_MAX_CMSG_FD
 */
extern int neb_sock_unix_send_with_fds(int fd, const char *data, int len, int *fds, int fd_num, void *name, socklen_t namelen)
	_nattr_warn_unused_result _nattr_nonnull((2, 4));
/**
 * \param[in] fds array to store received fd, should be *fd_num elements
 * \param[in,out] fd_num may be 0 after return, and must < NEB_UNIX_MAX_CMSG_FD
 * \return received datalen, or 0 if no data available, or -1 if error
 * \note close fds if fd_num is not zero
 * \note the fds returned will be cloexec
 */
extern int neb_sock_unix_recv_with_fds(int fd, char *data, int len, int *fds, int *fd_num)
	_nattr_warn_unused_result _nattr_nonnull((2, 4));

#endif
