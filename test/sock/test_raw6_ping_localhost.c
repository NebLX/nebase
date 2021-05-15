

#include <nebase/sock/raw.h>
#include <nebase/sock/inet.h>
#include <nebase/evdp/core.h>
#include <nebase/evdp/io_base.h>
#include <nebase/events.h>
#include <nebase/random.h>
#include <nebase/time.h>

#include <stdio.h>
#include <netinet/icmp6.h>
#include <arpa/inet.h>
#include <string.h>

static neb_evdp_queue_t evdp_queue = NULL;
static neb_evdp_timer_t evdp_timer = NULL;

static neb_evdp_timer_point send_tp = NULL;
static neb_evdp_timer_point quit_tp = NULL;

static int has_error = 0;
static int raw_fd = -1;

static uint16_t sent_icmp_id = 0;
static uint16_t sent_icmp_seq = 0;

static struct in6_addr loopback_addr = IN6ADDR_LOOPBACK_INIT;

struct ipv6_data {
	struct sockaddr_in6 peer_addr;
	struct sockaddr_in6 local_addr;
	struct timespec ts;
	unsigned int ifindex;
};

static neb_evdp_timeout_ret_t send_pkt(void *udata _nattr_unused)
{
	fprintf(stdout, "sending ...\n");

	u_char buf[64] = NEB_STRUCT_INITIALIZER;
	struct icmp6_hdr *ih = (struct icmp6_hdr *)buf;
	ih->icmp6_type = ICMP6_ECHO_REQUEST;
	ih->icmp6_code = 0;
	ih->icmp6_id = neb_random_uniform(UINT16_MAX);
	ih->icmp6_seq = neb_random_uniform(UINT16_MAX);
	sent_icmp_id = ih->icmp6_id;
	sent_icmp_seq = ih->icmp6_seq;
	// no need to calc checksum

	ssize_t nw = neb_sock_raw_icmp6_send(raw_fd, buf, sizeof(buf), &loopback_addr, &loopback_addr, 0);
	if (nw <= 0) {
		fprintf(stderr, "failed to send, nw: %zd\n", nw);
		has_error = 1;
		thread_events |= T_E_QUIT;
	}

	return NEB_EVDP_TIMEOUT_KEEP;
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

static neb_evdp_cb_ret_t recv_pkt(int fd, void *udata _nattr_unused, const void *context _nattr_unused)
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

	fprintf(stdout, "recieved one icmp echo reply packet\n");
	if (memcmp(&d.peer_addr.sin6_addr, &loopback_addr, sizeof(struct in6_addr)) != 0 ||
	    ih->icmp6_id != sent_icmp_id ||
	    ih->icmp6_seq != sent_icmp_seq)
		return NEB_EVDP_CB_CONTINUE;

	char peer_addr_s[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET6, &d.peer_addr.sin6_addr, peer_addr_s, sizeof(peer_addr_s));
	char local_addr_s[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET6, &d.local_addr.sin6_addr, local_addr_s, sizeof(local_addr_s));
	fprintf(stdout, "%llds %ldns %s <- %s, ifindex %u, size %zu\n",
		neb_time_sec_ll(d.ts.tv_sec), neb_time_nsec_l(d.ts.tv_nsec),
		local_addr_s, peer_addr_s, d.ifindex, left);

	if (d.ifindex == 0) {
		has_error = 1;
		fprintf(stderr, "no ifindex set\n");
	}

	return NEB_EVDP_CB_BREAK_EXP;
}

static neb_evdp_cb_ret_t fd_hup(int fd, void *udata, const void *context)
{
	neb_evdp_sock_log_on_hup(fd, udata, context);
	has_error = 1;
	return NEB_EVDP_CB_BREAK_ERR;
}

static neb_evdp_timeout_ret_t quit_timeout(void *udata _nattr_unused)
{
	thread_events |= T_E_QUIT;
	has_error = 1;
	fprintf(stderr, "quiting after timeout\n");
	return NEB_EVDP_TIMEOUT_KEEP;
}

int main(void)
{
	int ret = 0;

	evdp_queue = neb_evdp_queue_create(0);
	if (!evdp_queue) {
		fprintf(stderr, "failed to create evdp queue\n");
		goto exit_fail;
	}

	evdp_timer = neb_evdp_timer_create(10, 10);
	if (!evdp_timer) {
		fprintf(stderr, "failed to create evdp timer\n");
		goto exit_fail;
	}
	neb_evdp_queue_set_timer(evdp_queue, evdp_timer);

	send_tp = neb_evdp_timer_new_point(evdp_timer, 0, send_pkt, NULL);
	neb_evdp_queue_update_cur_msec(evdp_queue);
	int64_t quit_time = neb_evdp_queue_get_abs_timeout(evdp_queue, 5000);
	quit_tp = neb_evdp_timer_new_point(evdp_timer, quit_time, quit_timeout, NULL);

	raw_fd = neb_sock_raw_icmp6_new();
	if (raw_fd < 0) {
		fprintf(stderr, "failed to create raw socket\n");
		goto exit_fail;
	}

	neb_evdp_source_t s = neb_evdp_source_new_ro_fd(raw_fd, recv_pkt, fd_hup);
	if (!s) {
		fprintf(stderr, "failed to create source for the raw socket\b");
		goto exit_fail;
	}
	neb_evdp_source_set_on_remove(s, neb_evdp_source_del);
	if (neb_evdp_queue_attach(evdp_queue, s) != 0) {
		fprintf(stderr, "failed to attach source\n");
		neb_evdp_source_del(s);
		goto exit_fail;
	}

	if (neb_evdp_queue_run(evdp_queue) != 0) {
		fprintf(stderr, "error occure while running evdp queue\n");
		goto exit_fail;
	}
	if (has_error)
		ret = -1;

exit_clean:
	if (evdp_timer)
		neb_evdp_timer_destroy(evdp_timer);
	if (evdp_queue)
		neb_evdp_queue_destroy(evdp_queue);
	return ret;
exit_fail:
	ret = -1;
	goto exit_clean;
}

