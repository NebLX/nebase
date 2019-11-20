
#include <nebase/sock/raw.h>
#include <nebase/sock/inet.h>
#include <nebase/sock/csum.h>
#include <nebase/evdp.h>
#include <nebase/events.h>
#include <nebase/random.h>
#include <nebase/time.h>

#include <stdio.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>

static neb_evdp_queue_t evdp_queue = NULL;
static neb_evdp_timer_t evdp_timer = NULL;

static neb_evdp_timer_point send_tp = NULL;
static neb_evdp_timer_point quit_tp = NULL;

static int has_error = 0;
static int raw_fd = -1;

static uint16_t sent_icmp_id = 0;
static uint16_t sent_icmp_seq = 0;

struct ipv4_data {
	struct sockaddr_in peer_addr;
	struct sockaddr_in local_addr;
	struct timespec ts;
	unsigned int ifindex;
};

static neb_evdp_timeout_ret_t send_pkt(void *udata _nattr_unused)
{
	fprintf(stdout, "sending ...\n");

	u_char buf[64] = NEB_STRUCT_INITIALIZER;
	struct icmp *ih = (struct icmp *)buf;
	ih->icmp_type = ICMP_ECHO;
	ih->icmp_code = 0;
	ih->icmp_id = neb_random_uniform(UINT16_MAX);
	ih->icmp_seq = neb_random_uniform(UINT16_MAX);
	sent_icmp_id = ih->icmp_id;
	sent_icmp_seq = ih->icmp_seq;

	neb_sock_csum_icmp4_fill(ih, sizeof(buf));

	struct in_addr local;
	local.s_addr = htonl(INADDR_LOOPBACK);
	ssize_t nw = neb_sock_raw_icmp4_send(raw_fd, buf, sizeof(buf), &local, &local);
	if (nw <= 0) {
		fprintf(stderr, "failed to send, nw: %zd\n", nw);
		has_error = 1;
		thread_events |= T_E_QUIT;
	}

	return NEB_EVDP_TIMEOUT_KEEP;
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

static neb_evdp_cb_ret_t recv_pkt(int fd, void *udata _nattr_unused, const void *context _nattr_unused)
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
	size_t pktlen = neb_sock_raw4_get_pktlen(iphdr);
	if (pktlen > left) {
		fprintf(stderr, "Invalid IPv4 msg: pkt len %zu is larger than read size %zu\n", pktlen, left);
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

	fprintf(stdout, "recieved one icmp echo reply packet\n");
	if (iphdr->ip_dst.s_addr != htonl(INADDR_LOOPBACK) ||
	    icmphdr->icmp_id != sent_icmp_id ||
	    icmphdr->icmp_seq != sent_icmp_seq)
		return NEB_EVDP_CB_CONTINUE;

	char peer_addr_s[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &d.peer_addr.sin_addr, peer_addr_s, sizeof(peer_addr_s));
	char local_addr_s[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &d.local_addr.sin_addr, local_addr_s, sizeof(local_addr_s));
	fprintf(stdout, "%lds %ldns %s <- %s, ifindex %u, size %zu\n",
		d.ts.tv_sec, d.ts.tv_nsec, local_addr_s, peer_addr_s, d.ifindex, left);
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

	raw_fd = neb_sock_raw_icmp4_new();
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
