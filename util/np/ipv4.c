
#include <nebase/evdp.h>
#include <nebase/sock/raw.h>
#include <nebase/sock/inet.h>

#include "ipv4.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

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

static neb_evdp_cb_ret_t on_recv(int fd, void *udata _nattr_unused, const void *context _nattr_unused)
{
	static char buf[UINT16_MAX];

	struct sockaddr_in addr = {.sin_family = AF_INET};
	struct timespec ts;
	int nr = neb_sock_inet_recv_with_time(fd, (struct sockaddr *)&addr, buf, sizeof(buf), &ts);
	if (nr == -1) {
		fprintf(stderr, "failed to recv icmp packet with timestamp\n");
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

	left -= iphdr_len;
	if (left < (int)sizeof(struct icmp)) {
		fprintf(stderr, "Invalid ICMP msg: no valid header\n");
		return NEB_EVDP_CB_CONTINUE;
	}
	struct icmp *icmphdr = (struct icmp *)(buf + iphdr_len);
	if (icmphdr->icmp_type != ICMP_ECHOREPLY)
		return NEB_EVDP_CB_CONTINUE;

	char addr_s[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &addr.sin_addr, addr_s, sizeof(addr_s));
	fprintf(stdout, "%ld s %ld ns: received msg from %s of size %zu\n", ts.tv_sec, ts.tv_nsec, addr_s, left);
	return NEB_EVDP_CB_CONTINUE;
}

int np_ipv4_init(neb_evdp_queue_t q)
{
	ipv4_raw_fd = neb_sock_raw_icmp_new();
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
