
#include <nebase/evdp/base.h>
#include <nebase/evdp/helper.h>
#include <nebase/sock/raw.h>
#include <nebase/sock/inet.h>
#include <nebase/time.h>

#include "ipv6.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

struct ipv6_data {
	struct sockaddr_in6 peer_addr;
	struct sockaddr_in6 local_addr;
	struct timespec ts;
	unsigned int ifindex;
};

static int ipv6_raw_fd = -1;

static int on_remove(neb_evdp_source_t s)
{
	if (ipv6_raw_fd >= 0) {
		close(ipv6_raw_fd);
		ipv6_raw_fd = -1;
	}
	neb_evdp_source_del(s);
	return 0;
}

static int parse_cmsg(int level, int type, const u_char *data, size_t len _nattr_unused, void *udata)
{
	struct ipv6_data *d = udata;

	switch (level) {
	case NEB_CMSG_LEVEL_COMPAT:
		switch (type) {
		case NEB_CMSG_TYPE_LOOP_END:
			if (!d->ts.tv_sec)
				return neb_time_gettimeofday(&d->ts);
			break;
		case NEB_CMSG_TYPE_TIMESTAMP:
		{
			const struct timespec *t = (const struct timespec *)data;
			d->ts.tv_sec = t->tv_sec;
			d->ts.tv_nsec = t->tv_nsec;
		}
			break;
		default:
			break;
		}
		break;
	case IPPROTO_IPV6:
		switch (type) {
		case IPV6_PKTINFO:
		{
			const struct in6_pktinfo *info = (const struct in6_pktinfo *)data;
			d->local_addr.sin6_family = AF_INET6;
			memcpy(&d->local_addr.sin6_addr, &info->ipi6_addr, sizeof(struct in6_addr));
			d->ifindex = info->ipi6_ifindex;
		}
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	return 0;
}

static neb_evdp_cb_ret_t on_recv(int fd, void *udata _nattr_unused, const void *context _nattr_unused)
{
	static char buf[UINT16_MAX];

	struct ipv6_data d = NEB_STRUCT_INITIALIZER;
	d.peer_addr.sin6_family = AF_INET6;

	struct iovec iov = {.iov_base = buf, .iov_len = sizeof(buf)};

	struct neb_sock_msghdr m = {
		.msg_peer = (struct sockaddr *)&d.peer_addr,
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control_cb = parse_cmsg,
		.msg_udata = &d,
	};

	ssize_t nr = neb_sock_inet_recvmsg(fd, &m);
	if (nr == -1) {
		fprintf(stderr, "failed to recv icmp packet\n");
		return NEB_EVDP_CB_BREAK_ERR;
	}
	size_t left = nr;

	if (left < (int)sizeof(struct icmp6_hdr)) {
		fprintf(stderr, "Invalid ICMPv6 msg: no valid header\n");
		return NEB_EVDP_CB_CONTINUE;
	}
	struct icmp6_hdr *ih = (struct icmp6_hdr *)buf;
	if (ih->icmp6_type != ICMP6_ECHO_REPLY)
		return NEB_EVDP_CB_CONTINUE;

	char peer_addr_s[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET6, &d.peer_addr.sin6_addr, peer_addr_s, sizeof(peer_addr_s));
	char local_addr_s[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET6, &d.local_addr.sin6_addr, local_addr_s, sizeof(local_addr_s));
	fprintf(stdout, "%llds %ldns %s <- %s, ifindex %u, size %zu\n",
		(long long)d.ts.tv_sec, d.ts.tv_nsec, local_addr_s, peer_addr_s, d.ifindex, left);
	return NEB_EVDP_CB_CONTINUE;
}

int np_ipv6_init(neb_evdp_queue_t q)
{
	ipv6_raw_fd = neb_sock_raw_icmp6_new();
	if (ipv6_raw_fd == -1) {
		perror("failed to create ICMPv6 raw socket\n");
		return -1;
	}
	if (neb_sock_inet_enable_recv_time(ipv6_raw_fd) != 0) {
		fprintf(stderr, "failed to enable the receive of timestamp\n");
		close(ipv6_raw_fd);
		return -1;
	}
	struct icmp6_filter filter;
	ICMP6_FILTER_SETBLOCKALL(&filter);
	ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY, &filter);
	if (setsockopt(ipv6_raw_fd, IPPROTO_ICMPV6, ICMP6_FILTER, &filter, sizeof(filter)) == -1) {
		perror("setsockopt(IPPROTO_ICMPV6/ICMP6_FILTER)");
		close(ipv6_raw_fd);
		return -1;
	}

	neb_evdp_source_t s = neb_evdp_source_new_ro_fd(ipv6_raw_fd, on_recv, neb_evdp_sock_log_on_hup);
	if (!s) {
		fprintf(stderr, "failed to create evdp source\n");
		close(ipv6_raw_fd);
		return -1;
	}
	neb_evdp_source_set_on_remove(s, on_remove);
	if (neb_evdp_queue_attach(q, s) != 0) {
		fprintf(stderr, "failed to attach evdp source to queue\n");
		neb_evdp_source_del(s);
		close(ipv6_raw_fd);
		return -1;
	}

	return 0;
}
