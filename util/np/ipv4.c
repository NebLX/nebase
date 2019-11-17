
#include <nebase/evdp.h>
#include <nebase/sock/raw.h>
#include <nebase/sock/inet.h>
#include <nebase/time.h>

#include "ipv4.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

struct ipv4_data {
	struct sockaddr_in peer_addr;
	struct sockaddr_in local_addr;
	struct timespec ts;
	unsigned int ifindex;
};

static int ipv4_raw_fd = -1;

static int on_remove(neb_evdp_source_t s)
{
	if (ipv4_raw_fd >= 0) {
		close(ipv4_raw_fd);
		ipv4_raw_fd = -1;
	}
	neb_evdp_source_del(s);
	return 0;
}

static int parse_cmsg(int level, int type, const u_char *data, size_t len _nattr_unused, void *udata)
{
	struct ipv4_data *d = udata;

	if (level != NEB_CMSG_LEVEL_COMPAT)
		return 0;

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
	case NEB_CMSG_TYPE_IP4IFINDEX:
		d->ifindex = *(const int *)data;
		break;
	default:
		break;
	}

	return 0;
}

static neb_evdp_cb_ret_t on_recv(int fd, void *udata _nattr_unused, const void *context _nattr_unused)
{
	static char buf[UINT16_MAX];

	struct ipv4_data d = NEB_STRUCT_INITIALIZER;
	d.peer_addr.sin_family = AF_INET;

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

	if (left < (int)sizeof(struct ip)) {
		fprintf(stderr, "Invalid IPv4 msg: no valid header\n");
		return NEB_EVDP_CB_CONTINUE;
	}
	struct ip *iphdr = (struct ip *)buf;
	size_t pktlen = ntohs(iphdr->ip_len);
	if (pktlen > left) {
		fprintf(stderr, "Invalid IPv4 msg: pkglen %zu is larger than read size %zu\n", pktlen, left);
		return NEB_EVDP_CB_CONTINUE;
	}
	size_t iphdr_len = iphdr->ip_hl << 2;
	if (iphdr_len > left) {
		fprintf(stderr, "Invalid IPv4 msg: hdr len %zu is larger than read size %zu\n", iphdr_len, left);
		return NEB_EVDP_CB_CONTINUE;
	}
	d.local_addr.sin_family = AF_INET;
	d.local_addr.sin_addr.s_addr = iphdr->ip_dst.s_addr;

	left -= iphdr_len;
	if (left < (int)sizeof(struct icmp)) {
		fprintf(stderr, "Invalid ICMP msg: no valid header\n");
		return NEB_EVDP_CB_CONTINUE;
	}
	struct icmp *icmphdr = (struct icmp *)(buf + iphdr_len);
	if (icmphdr->icmp_type != ICMP_ECHOREPLY)
		return NEB_EVDP_CB_CONTINUE;

	// handle d

	char peer_addr_s[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &d.peer_addr.sin_addr, peer_addr_s, sizeof(peer_addr_s));
	char local_addr_s[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &d.local_addr.sin_addr, local_addr_s, sizeof(local_addr_s));
	fprintf(stdout, "%lds %ldns %s <- %s, ifindex %u, size %zu\n",
		d.ts.tv_sec, d.ts.tv_nsec, local_addr_s, peer_addr_s, d.ifindex, left);
	return NEB_EVDP_CB_CONTINUE;
}

int np_ipv4_init(neb_evdp_queue_t q)
{
	ipv4_raw_fd = neb_sock_raw_icmp4_new();
	if (ipv4_raw_fd == -1) {
		perror("failed to create ICMP raw socket\n");
		return -1;
	}
	if (neb_sock_inet_enable_recv_time(ipv4_raw_fd) != 0) {
		fprintf(stderr, "failed to enable the receive of timestamp\n");
		close(ipv4_raw_fd);
		return -1;
	}

	neb_evdp_source_t s = neb_evdp_source_new_ro_fd(ipv4_raw_fd, on_recv, neb_evdp_sock_log_on_hup);
	if (!s) {
		fprintf(stderr, "failed to create evdp source\n");
		close(ipv4_raw_fd);
		return -1;
	}
	neb_evdp_source_set_on_remove(s, on_remove);
	if (neb_evdp_queue_attach(q, s) != 0) {
		fprintf(stderr, "failed to attach evdp source to queue\n");
		neb_evdp_source_del(s);
		close(ipv4_raw_fd);
		return -1;
	}

	return 0;
}
