
#include <nebase/syslog.h>

#include "sock_linux.h"

#include <unistd.h>
#include <sys/socket.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/sock_diag.h>
#include <linux/unix_diag.h>
#include <netinet/tcp.h>

/* macros get from include/linux/kdev_t.h */
#define MINORBITS	20
#define MINORMASK	((1U << MINORBITS) - 1)

#define MAJOR(dev)	((unsigned int) ((dev) >> MINORBITS))
#define MINOR(dev)	((unsigned int) ((dev) & MINORMASK))
#define MKDEV(ma,mi)	(((ma) << MINORBITS) | (mi))

static int unix_diag_send_query_vfs(int fd)
{
	struct sockaddr_nl nladdr = {
		.nl_family = AF_NETLINK
	};
	struct {
		struct nlmsghdr nlh;
		struct unix_diag_req udr;
	} req = {
		.nlh = {
			.nlmsg_len = sizeof(req),
			.nlmsg_type = SOCK_DIAG_BY_FAMILY,
			.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP
		},
		.udr = {
			.sdiag_family = AF_UNIX,
			.udiag_states = -1,
			.udiag_show = UDIAG_SHOW_VFS
		}
	};
	struct iovec iov = {
		.iov_base = &req,
		.iov_len = sizeof(req)
	};
	struct msghdr msg = {
		.msg_name = &nladdr,
		.msg_namelen = sizeof(nladdr),
		.msg_iov = &iov,
		.msg_iovlen = 1
	};
	if (sendmsg(fd, &msg, 0) == -1) {
		neb_syslog(LOG_ERR, "sendmsg: %m");
		return -1;
	}
	return 0;
}

int neb_sock_unix_get_ino(const neb_ino_t *fs_ni, ino_t *sock_ino, int *type)
{
	*sock_ino = 0;
	*type = 0;
	int ret = -1;
	int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_INET_DIAG);
	if (fd == -1) {
		neb_syslog(LOG_ERR, "socket: %m");
		return ret;
	}

	if (unix_diag_send_query_vfs(fd) != 0) {
		neb_syslog(LOG_ERR, "Failed to send unix_diag query request");
		goto exit_return;
	}

	char buf[8192];
	struct sockaddr_nl nladdr = {
		.nl_family = AF_NETLINK
	};
	struct iovec iov = {
		.iov_base = buf,
		.iov_len = sizeof(buf)
	};

	for (;;) {
		struct msghdr msg = {
			.msg_name = &nladdr,
			.msg_namelen = sizeof(nladdr),
			.msg_iov = &iov,
			.msg_iovlen = 1
		};

		ssize_t nr = recvmsg(fd, &msg, 0);
		switch (nr) {
		case -1:
			neb_syslog(LOG_ERR, "recvmsg: %m"); // fall through
		case 0:
			goto exit_return;
			break;
		default:
			break;
		}

		const struct nlmsghdr *h = (struct nlmsghdr *)buf;
		if (!NLMSG_OK(h, nr)) {
			neb_syslog(LOG_ERR, "!NLMSG_OK");
			goto exit_return;
		}

		for (; NLMSG_OK(h, nr); h = NLMSG_NEXT(h, nr)) {
			switch (h->nlmsg_type) {
			case SOCK_DIAG_BY_FAMILY:
				break;
			case NLMSG_ERROR:
			{
				const struct nlmsgerr *err = NLMSG_DATA(h);
				if (h->nlmsg_len < NLMSG_LENGTH(sizeof(*err))) {
					neb_syslog(LOG_ERR, "nlmsg_error: invalid recv size");
				} else {
					neb_syslog_en(-err->error, LOG_ERR, "nlmsg_error: %m");
				}
				goto exit_return;
			}
				break;
			case NLMSG_DONE:
				ret = 0;
				goto exit_return;
				break;
			default:
				neb_syslog(LOG_ERR, "unexpected nlmsg_type %u", h->nlmsg_type);
				goto exit_return;
				break;
			}

			const struct unix_diag_msg *diag = NLMSG_DATA(h);
			if (h->nlmsg_len < NLMSG_LENGTH(sizeof(*diag))) {
				neb_syslog(LOG_ERR, "unix_diag_msg: invalid recv size");
				goto exit_return;
			}
			if (diag->udiag_family != AF_UNIX) {
				neb_syslog(LOG_ERR, "unix_diag_msg: invalid family %u", diag->udiag_family);
				goto exit_return;
			}

			struct rtattr *attr;
			unsigned int rta_len = h->nlmsg_len - NLMSG_LENGTH(sizeof(*diag));
			for (attr = (struct rtattr *)(diag + 1); RTA_OK(attr, rta_len); attr = RTA_NEXT(attr, rta_len)) {
				switch (attr->rta_type) {
				case UNIX_DIAG_VFS:
				{
					struct unix_diag_vfs *vfs = RTA_DATA(attr);
					if (attr->rta_len < RTA_LENGTH(sizeof(*vfs))) {
						neb_syslog(LOG_ERR, "unix_diag_vfs: invalid recv size");
						goto exit_return;
					}
					int dev_major = MAJOR(vfs->udiag_vfs_dev);
					int dev_minor = MINOR(vfs->udiag_vfs_dev);
					if (dev_major == fs_ni->dev_major &&
					    dev_minor == fs_ni->dev_minor &&
					    vfs->udiag_vfs_ino == fs_ni->ino) {
						*sock_ino = diag->udiag_ino;
						*type = diag->udiag_type;
						ret = 0;
						goto exit_return;
					}
				}
					break;
				default:
					break;
				}
			}
		}
	}

exit_return:
	close(fd);
	return ret;
}
