

#include <nebase/sock/raw.h>
#include <nebase/evdp.h>
#include <nebase/events.h>
#include <nebase/random.h>

#include <stdio.h>
#include <netinet/icmp6.h>

static neb_evdp_queue_t evdp_queue = NULL;
static neb_evdp_timer_t evdp_timer = NULL;

static neb_evdp_timer_point send_tp = NULL;
static neb_evdp_timer_point quit_tp = NULL;

static int has_error = 0;
static int raw_fd = -1;

static neb_evdp_timeout_ret_t send_pkt(void *udata _nattr_unused)
{
	fprintf(stdout, "sending ...\n");

	u_char buf[64] = NEB_STRUCT_INITIALIZER;
	struct icmp6_hdr *ih = (struct icmp6_hdr *)buf;
	ih->icmp6_type = ICMP6_ECHO_REQUEST;
	ih->icmp6_code = 0;
	ih->icmp6_id = neb_random_uniform(UINT16_MAX);
	ih->icmp6_seq = neb_random_uniform(UINT16_MAX);
	// no need to calc checksum

	ssize_t nw = neb_sock_raw_icmp6_send(raw_fd, buf, sizeof(buf), &in6addr_loopback, &in6addr_loopback, 0);
	if (nw <= 0) {
		fprintf(stderr, "failed to send, nw: %zd\n", nw);
		has_error = 1;
		thread_events |= T_E_QUIT;
	}

	return NEB_EVDP_TIMEOUT_KEEP;
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

	// TODO

	if (neb_evdp_queue_run(evdp_queue) != 0) {
		fprintf(stderr, "error occure while running evdp queue\n");
		goto exit_fail;
	}
	if (has_error)
		ret = -1;

exit_clean:
	if (evdp_timer) {
		if (send_tp)
			neb_evdp_timer_del_point(evdp_timer, send_tp);
		if (quit_tp)
			neb_evdp_timer_del_point(evdp_timer, quit_tp);
		neb_evdp_timer_destroy(evdp_timer);
	}
	if (evdp_queue)
		neb_evdp_queue_destroy(evdp_queue);
	return ret;
exit_fail:
	ret = -1;
	goto exit_clean;
}

