
#include <nebase/evdp.h>
#include <nebase/sock/inet.h>

#include "ipv6.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

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

static neb_evdp_cb_ret_t on_recv(int fd, void *udata _nattr_unused, const void *context _nattr_unused)
{
	static char buf[UINT16_MAX];

	struct sockaddr_in6 addr = {.sin6_family = AF_INET6};
	struct timespec ts;
	int nr = neb_sock_inet_recv_with_time(fd, (struct sockaddr *)&addr, buf, sizeof(buf), &ts);
	if (nr == -1) {
		fprintf(stderr, "failed to recv icmp packet with timestamp\n");
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

	char addr_s[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET6, &addr.sin6_addr, addr_s, sizeof(addr_s));
	fprintf(stdout, "%ld s %ld ns: received msg from %s of size %zu\n", ts.tv_sec, ts.tv_nsec, addr_s, left);
	return NEB_EVDP_CB_CONTINUE;
}

int np_ipv6_init(neb_evdp_queue_t q)
{
	ipv6_raw_fd = neb_sock_inet_new(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
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
		fprintf(stderr, "setsockopt(IPPROTO_ICMPV6/ICMP6_FILTER): %m");
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
