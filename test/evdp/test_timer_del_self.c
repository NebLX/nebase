
#include <nebase/cdefs.h>
#include <nebase/evdp.h>
#include <nebase/events.h>

#include <stdio.h>

static int run_into_sys_timer = 0;

static void *timer_point = NULL;

static void timer_cb(void *udata)
{
	neb_evdp_timer_t t = udata;
	if (timer_point) {
		fprintf(stderr, "ok: running del self timer\n");
		neb_evdp_timer_del(t, timer_point);
		thread_events |= T_E_QUIT;
	} else {
		fprintf(stderr, "error: not running del self timer\n");
	}
}

static neb_evdp_cb_ret_t sys_timer_cb(unsigned int id _nattr_unused, long overrun _nattr_unused, void *data _nattr_unused)
{
	fprintf(stderr, "run into sys timer callback\n");
	run_into_sys_timer = 1;
	return NEB_EVDP_CB_BREAK_EXP;
}

int main(void)
{
	neb_evdp_queue_t q = neb_evdp_queue_create(0);
	if (!q) {
		fprintf(stderr, "failed to create evdp queue\n");
		return -1;
	}

	int ret = 0;
	neb_evdp_source_t s = neb_evdp_source_new_itimer_ms(0, 100, sys_timer_cb);
	if (!s) {
		fprintf(stderr, "failed to create sys timer source\n");
		ret = -1;
		goto exit_destroy_queue;
	}
	if (neb_evdp_queue_attach(q, s) != 0) {
		fprintf(stderr, "failed to add sys timer source to queue\n");
		ret = -1;
		goto exit_destroy_sys_timer;
	}

	neb_evdp_timer_t t = neb_evdp_timer_create(1, 1);
	if (!t) {
		fprintf(stderr, "failed to create evdp timer\n");
		ret = -1;
		goto exit_detach_sys_timer;
	}
	neb_evdp_queue_set_timer(q, t);

	timer_point = neb_evdp_timer_add(t, 1, timer_cb, t);
	if (!timer_point) {
		fprintf(stderr, "failed to add internal timer\n");
		ret = -1;
		goto exit_destroy_timer;
	}

	if (neb_evdp_queue_run(q) != 0) {
		fprintf(stderr, "error occured while running evdp queue\n");
		ret = -1;
	}

	if (run_into_sys_timer)
		ret = -1;

	neb_evdp_timer_del(t, timer_point);
exit_destroy_timer:
	neb_evdp_timer_destroy(t);
exit_detach_sys_timer:
	neb_evdp_queue_detach(q, s);
exit_destroy_sys_timer:
	neb_evdp_source_del(s);
exit_destroy_queue:
	neb_evdp_queue_destroy(q);
	return ret;
}
