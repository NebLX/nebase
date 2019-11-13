
#include <nebase/evdp.h>
#include <nebase/sock.h>

#include "ipv4.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>

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

	struct timeval tv;
	int nr = neb_sock_net_recv_with_timeval(fd, buf, sizeof(buf), &tv);
	if (nr == -1) {
		fprintf(stderr, "failed to recv icmp packet with timestamp");
		return NEB_EVDP_CB_BREAK_ERR;
	}

	fprintf(stdout, "%ld s %ld us: received msg of size %d\n", tv.tv_sec, tv.tv_usec, nr);
	return NEB_EVDP_CB_BREAK_EXP;
}

int np_ipv4_init(neb_evdp_queue_t q)
{
	ipv4_raw_fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (ipv4_raw_fd == -1) {
		perror("socket");
		return -1;
	}
	int enable_timestamp = 1;
	if (setsockopt(ipv4_raw_fd, SOL_SOCKET, SO_TIMESTAMP, &enable_timestamp, sizeof(int)) == -1) {
		perror("socketopt(SO_TIMESTAMP)");
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
